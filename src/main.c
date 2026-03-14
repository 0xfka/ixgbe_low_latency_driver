#include <errno.h>
#include <linux/if_ether.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <xmmintrin.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <x86intrin.h>
#include "hdr/hdr_histogram.h"
#include <sched.h>
#include <sys/mman.h>

#include "../selftests/selftests.h"
#include "base.h"
#include "hw.h"
#include "ixgbe.h"
#include "pci.h"
#include "datapath.h"
struct hw ixgbe_adapter __attribute__((aligned(64))) = {0};
static struct ixgbe_stats stats = {0};
volatile sig_atomic_t run = true;
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
  err = (mlockall(MCL_CURRENT | MCL_FUTURE));
  if (unlikely(err != 0)) {
    write(STDERR_FILENO,"mlockall failed\n",16);
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
  struct hdr_histogram* latency_hist;
  hdr_init(1, 10000000, 3, &latency_hist); 
  u32 tx_write = 0; /* Next to write */
  u32 tx_clean = 0; /* Oldest to clean */
  while(run){
    barrier();
    if(likely(rx_ring[i].wb.status_error & IXGBE_RXD_STAT_DD)){
      rmb();
      u64 start_cycles, end_cycles;
      start_cycles = __rdtsc();
      stats.batch_manage_tail_counter++;
      stats.total_packets++;
      if(unlikely(((tx_clean - tx_write -1) & (BUFFER_NUMBER - 1))<= stats.batch_manage_tail)){
        while (tx_ring[tx_clean].data_wb.sta & IXGBE_RXD_STAT_DD) {
        tx_ring[tx_clean].data_wb.sta &= ~IXGBE_RXD_STAT_DD;
        tx_clean = (tx_clean + 1) & (BUFFER_NUMBER -1);
        }
      }
      /* Packet parsing logic is added temporarily to prove pointer arithmatics on structures.
      * Since the driver cannot reply ARP's, static ARP configuration needed.
      */
      u8* pkt = (u8*)ixgbe_adapter.rx_base + (256 * 1024) + ( i * 2048);
      u16 pkt_len = rx_ring[i].wb.length;
      if(unlikely(pkt_len < sizeof(struct ethhdr) +sizeof(struct iphdr))){
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        stats.drop++;
        wmb();
        i = (i + 1) & (BUFFER_NUMBER -1);
        continue;
      }
      struct ethhdr *eth = (struct ethhdr *)pkt;
      struct iphdr *ip = (struct iphdr *)(pkt + sizeof(struct ethhdr));
      u16 given_len = ip->ihl *4;
      if(unlikely(given_len < sizeof(struct iphdr) || given_len > 60)){
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        stats.drop++;
        wmb();
        i = (i + 1) & (BUFFER_NUMBER -1);
        continue;
      }
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
      if(unlikely((((tx_write + 1) & (BUFFER_NUMBER - 1)) == tx_clean))){
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        i = (i + 1) & ( BUFFER_NUMBER -1 );
        stats.drop++;
        wmb();
        continue;
      }
      struct icmphdr *icmp = NULL;
      u16 icmp_check = sizeof(struct ethhdr) + given_len + sizeof(struct icmphdr);
      if(unlikely(pkt_len < icmp_check)){
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        stats.drop++;
        wmb();
        i = (i + 1) & (BUFFER_NUMBER -1);
        continue;
      }
      if(likely(pass == true)) {icmp = (struct icmphdr *)(pkt + sizeof(struct ethhdr) + ip->ihl * 4);}
      if(likely(pass == true)) {processed = ping_reply(eth, ip, icmp, &stats, tx_write, rx_ring, tx_ring);}
      wmb();
      if(unlikely(!processed)){
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        wmb();
        i = (i + 1) & (BUFFER_NUMBER -1);
        continue;
      }
      if(unlikely(pass != processed)){
        /* impossible condition, proves that there's a huge logical error in fw logic. */
        return EPIPE;
      }
      rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
      rx_ring[i].read.pkt_addr = (u64)ixgbe_adapter.rx_base_phy + (256 * 1024 ) + (tx_write * 2048);
      rx_ring[i].read.hdr_addr = 0;
      wmb();
      i = (i + 1) & (BUFFER_NUMBER - 1);
      tx_write = (tx_write + 1) & (BUFFER_NUMBER -1);
      if(unlikely(stats.batch_tx_counter >= stats.batch_tx_transmit)){
      ixgbe_write_reg(&ixgbe_adapter, IXGBE_TDT, tx_write);
      stats.batch_tx_counter = 0;
      } else {
      stats.batch_tx_counter++;
      }
      end_cycles = __rdtsc();
      hdr_record_value(latency_hist, end_cycles - start_cycles);      
      }
  }
  hdr_percentiles_print(latency_hist, stdout, 5, 1.0, CLASSIC);
}