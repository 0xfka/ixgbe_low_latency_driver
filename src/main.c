#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <xmmintrin.h>

#include "../selftests/selftests.h"
#include "base.h"
#include "hw.h"
#include "ixgbe.h"
#include "pci.h"
struct hw ixgbe_adapter __attribute__((aligned(64))) = {0};
union ixgbe_adv_rx_desc ixgbe_adv_rx_desc __attribute__((aligned(64))) = {0};
union ixgbe_adv_tx_desc ixgbe_adv_tx_desc __attribute__((aligned(64))) = {0};

int main(const int argc, char** argv) {
  /* Check data structures is valid or not before touching to device */
  if (unlikely(sizeof(ixgbe_adv_rx_desc) != 16)){
    printf("rx descriptor is not aligned. size is : %lu\n", sizeof(ixgbe_adv_rx_desc));
    return -EINVAL;
  }
  if (unlikely(sizeof(ixgbe_adv_tx_desc) != 16)){
    printf("tx descriptor is not aligned. size is : %lu\n", sizeof(ixgbe_adv_tx_desc));
    return -EINVAL;
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
  int err = unbind(ixgbe_adapter.pci_addr, "uio_pci_generic");
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
  u32 batch_manage_tail = 128;
  u32 batch_manage_tail_counter = 0;
  u32 total_packets = 0;
  u32 irrelevant_packets = 0;
  u32 batch_tx_counter = 0;
  /* Basic benchmarks show that batching Tx is increasing latency too much.
  * Benchmarks will be added before merging this branch to main.
  */
  u32 batch_tx_transmit = 0;
  u32 i = ixgbe_read_reg(&ixgbe_adapter, IXGBE_RDH);
  while(1){
    barrier();
    if(likely(rx_ring[i].wb.status_error & IXGBE_RXD_STAT_DD)){
      rmb();
      batch_manage_tail_counter++;
      total_packets++;
      /* This PoC only includes ICMP. */
      if(unlikely(rx_ring[i].wb.pkttype != 0x01)){
        rx_ring[i].wb.status_error = 0;
        i = IXGBE_BUFFER_ADVANCE(i, 1);
        irrelevant_packets++;
        continue;
      } 
      /* Packet parsing logic is added temporarily to prove pointer arithmatics on structures.
      * Since the driver cannot reply ARP's, static ARP configuration needed.
      */
      u8* pkt = (u8*)ixgbe_adapter.rx_base + (256 * 1024) + ( i * 2048);
      struct eth_hdr *eth = (struct eth_hdr *)pkt;
      /* ABOUT PRINT USAGES ON DATAPATH
      * Of course we are not going to use print on datapath,
      * but in the development of the ring buffer and ICMP logic, 
      * they are the only debugging data until we can hit packets to wire.
      * They'll not be removed completely since we may need them again, 
      * but including them will be decided on compile time,
      * which means they have zero affect to the datapath when compiled without 
      * related flag.
      */

      /*
      u8* src_mac = eth->src_mac;
      printf("source mac address: %0x:%0x:%0x:%0x:%0x:%0x\n", src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
      u8* dst_mac = eth->dst_mac;
      printf("destination mac address: %0x:%0x:%0x:%0x:%0x:%0x\n", dst_mac[0], dst_mac[1], dst_mac[2], dst_mac[3], dst_mac[4], dst_mac[5]);
      struct ip_hdr *ip = (struct ip_hdr *)(pkt + sizeof(struct eth_hdr));
      u32 src_ip = __builtin_bswap32(ip->src_addr); See little endian/big endian byte orders.
      printf("source ip addrress: %0x\n", src_ip);
      u32 dst_ip = __builtin_bswap32(ip->dst_addr);
      printf("destination ip address: %0x\n",dst_ip);
      if(unlikely(batch_manage_tail_counter == batch_manage_tail)){
        ixgbe_write_reg(&ixgbe_adapter, IXGBE_RDT, i);
        batch_manage_tail_counter = 0;
      }
      /* PoC datapath 
      */
      /* Swap MAC's */
      for (int j = 0; j < 6; j++) {
      u8 tmp = eth->src_mac[j];
      eth->src_mac[j] = eth->dst_mac[j];
      eth->dst_mac[j] = tmp;
      }
      /* Swap IP's */
      u32 *src_ip_ptr = (u32*)(pkt + 26);
      u32 *dst_ip_ptr = (u32*)(pkt + 30);
      u32 tmp_ip = *src_ip_ptr;
      *src_ip_ptr = *dst_ip_ptr;
      *dst_ip_ptr = tmp_ip;
      /* Change type to reply on ICMP */
      struct icmphdr *icmp = (struct icmphdr *)(pkt + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));
      if(likely(pkt[34] == 8)){
        pkt[34] = 0;
      /* Calcucate new checksum withto RFC 1624 */
      u32 chk_new = ntohs(icmp->checksum) + 0x0800;
      if (unlikely(chk_new > 0xFFFF))
        chk_new = (chk_new & 0xFFFF) +1;
      icmp->checksum = htons(chk_new);
      }
      u64 transmit = ixgbe_adapter.rx_base_phy + (256 * 1024) + ( i * 2048);
      union ixgbe_adv_tx_desc *tx_desc = &tx_ring[i];
      tx_desc->data_read.address = transmit;
      tx_desc->data_read.dtalen = 84;
      tx_desc->data_read.paylen = 84;
      tx_desc->data_read.dtyp = 3;
      tx_desc->data_read.rs = 1;
      tx_desc->data_read.eop = 1;
      tx_desc->data_read.ifcs = 1;
      tx_desc->data_read.dext = 1;
      tx_desc->data_read.dd = 0;
      wmb();
      /* Reset Descriptor Done */
      rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
      rx_ring[i].read.pkt_addr = (u64)ixgbe_adapter.rx_base_phy + (256 * 1024) + (i * 2048);
      rx_ring[i].read.hdr_addr = 0;
      wmb();
      i = IXGBE_BUFFER_ADVANCE(i, 1);
      if(unlikely(batch_tx_counter >= batch_tx_transmit)){
      ixgbe_write_reg(&ixgbe_adapter, IXGBE_TDT, i);
      batch_tx_counter = 0;
      } else {
      batch_tx_counter++;
      }
      /*
      printf("tail : %u\n", ixgbe_read_reg(&ixgbe_adapter,IXGBE_TDT));
      printf("head : %u\n", ixgbe_read_reg(&ixgbe_adapter,IXGBE_TDH));
      */
      }
  }
}