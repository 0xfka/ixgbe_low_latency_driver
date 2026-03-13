#include <errno.h>
#include <linux/if_ether.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <xmmintrin.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <x86intrin.h>

#include "../selftests/selftests.h"
#include "base.h"
#include "hw.h"
#include "ixgbe.h"
#include "pci.h"
#include "datapath.h"
struct hw ixgbe_adapter __attribute__((aligned(64))) = {0};
static struct ixgbe_stats stats = {0};
volatile sig_atomic_t run = true;
static u64 cycle_samples[99999999]; 
void handle_sigint(int sig) {
    run = false;
}
int main(const int argc, char** argv) {
  signal(SIGINT, handle_sigint);
  int err;
  err = ixgbe_test_ds();
  if (unlikely(err != 0)) {
    return -err;
  }
  if (unlikely(argc < 2)) {
    write(STDERR_FILENO,
          "usage: ./binary <pci_addr>. use lspci for PCI addr.\n", 52);
    return EINVAL;
  }
  if (unlikely(argv[1] == NULL)) {
    return EINVAL;
  }
  ixgbe_adapter.pci_addr = argv[1];
  // Driver should be changed for another PCI direct access modes.
  err = unbind(ixgbe_adapter.pci_addr, "uio_pci_generic");
  if (unlikely(err != 0)) {
    return -err;
  }
  err = alloc_hugepage(&ixgbe_adapter);
  if (unlikely(err != 0)) {
    return -err;
  }
  err = virt2phy((u64)ixgbe_adapter.rx_base,&ixgbe_adapter.rx_base_phy);
  if (unlikely(err != 0)) {
    return -err;
  }
  err = virt2phy((u64)ixgbe_adapter.tx_base,&ixgbe_adapter.tx_base_phy);
  if (unlikely(err != 0)) {
    return -err;
  }
  err = mmap_bar0(&ixgbe_adapter);
  if (unlikely(err != 0)) {
    return -err;
  }
  err = ixgbe_run_diagnostic(&ixgbe_adapter);
  if (unlikely(err != 0)) {
    return -err;
  }
  err = ixgbe_probe(&ixgbe_adapter);
  if (unlikely(err != 0)) {
    return -err;
  }
  union ixgbe_adv_rx_desc *rx_ring = (union ixgbe_adv_rx_desc *)ixgbe_adapter.rx_base;
  union ixgbe_adv_tx_desc *tx_ring = (union ixgbe_adv_tx_desc *)ixgbe_adapter.tx_base;
  ixgbe_write_reg(&ixgbe_adapter, IXGBE_RDT, BUFFER_NUMBER -1);
  ixgbe_read_reg(&ixgbe_adapter, IXGBE_GPRC);
  ixgbe_read_reg(&ixgbe_adapter, IXGBE_RXMPC);
  u32 read_val = ixgbe_read_reg(&ixgbe_adapter, IXGBE_AUTOC);
  IXGBE_SET_BITS(read_val, IXGBE_AUTOC_RESTART);
  ixgbe_write_reg(&ixgbe_adapter, IXGBE_AUTOC, read_val);
  /* This register is used for updating ring buffer location on every x bytes. 
  * 128 is a placeholder, a number will be decided after benchmarks. */
  stats.batch_manage_tail = 128;
  stats.batch_manage_tail_counter = 0;
  stats.batch_tx_counter = 0;
  /* Basic benchmarks show that batching Tx is increasing latency too much.
  * Benchmarks will be added before merging this branch to main.
  */
  stats.batch_tx_transmit = 0;
  u32 i = ixgbe_read_reg(&ixgbe_adapter, IXGBE_RDH);
  static u32 sample_idx = 0;
  while(run){
    barrier();
    if(likely(rx_ring[i].wb.status_error & IXGBE_RXD_STAT_DD)){
      rmb();
      u64 start_cycles, end_cycles;
      start_cycles = __rdtsc();
      stats.batch_manage_tail_counter++;
      stats.total_packets++;
      /* Packet parsing logic is added temporarily to prove pointer arithmatics on structures.
      * Since the driver cannot reply ARP's, static ARP configuration needed.
      */
      u8* pkt = (u8*)ixgbe_adapter.rx_base + (256 * 1024) + ( i * 2048);
      struct ethhdr *eth = (struct ethhdr *)pkt;
      struct iphdr *ip = (struct iphdr *)(pkt + sizeof(struct ethhdr));
      stats.total_bytes_rx = stats.total_bytes_rx + ip->tot_len;
      if(unlikely(stats.batch_manage_tail_counter >= stats.batch_manage_tail)){
        ixgbe_write_reg(&ixgbe_adapter, IXGBE_RDT, i);
        stats.batch_manage_tail_counter = 0;
      }
      bool pass = false;
      bool processed = false;
      /* Temporary hardcoded testing the logic 
       * Note that network byte orders are big endian, 
       * and the test value is the reverse of sender ip, which is used in local lab when development.
       * Firewalling logic will convert the IP addresses when rule is added, instead of reversing ip->saddr every time.
       */
      u32 allowed_ip = 0x0200000a;
      if(unlikely(ip->saddr != allowed_ip)){
        pass = false;
      }else {
      pass = true;
      }
      struct icmphdr *icmp = NULL;
      if(likely(pass == true)) {icmp = (struct icmphdr *)(pkt + sizeof(struct ethhdr) + ip->ihl * 4);}
      if(likely(pass == true)) {processed = ping_reply(eth, ip, icmp, &stats, i, rx_ring, tx_ring);}
      wmb();
      rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
      rx_ring[i].read.pkt_addr = (u64)ixgbe_adapter.rx_base_phy + (256 * 1024) + (i * 2048);
      rx_ring[i].read.hdr_addr = 0;
      wmb();
      if(unlikely(pass != processed)){
        /* impossible condition, proves that there's a huge logical error in fw logic. */
        return EPIPE;
      }
      i = IXGBE_BUFFER_ADVANCE(i, 1);
      if(unlikely(!processed)){
        continue;
      }
      if(unlikely(stats.batch_tx_counter >= stats.batch_tx_transmit)){
      ixgbe_write_reg(&ixgbe_adapter, IXGBE_TDT, i);
      stats.batch_tx_counter = 0;
      } else {
      stats.batch_tx_counter++;
      }
      end_cycles = __rdtsc();
      cycle_samples[sample_idx] = end_cycles - start_cycles;
      sample_idx++;
      }
  }
  for(u64 j = 0; j < (sample_idx ); j++) {
    printf("%lu\n", cycle_samples[j]);
}
}