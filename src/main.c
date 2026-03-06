#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../selftests/selftests.h"
#include "base.h"
#include "hw.h"
#include "ixgbe.h"
#include "pci.h"
struct hw ixgbe_adapter __attribute__((aligned(64))) = {0};
union ixgbe_adv_rx_desc ixgbe_adv_rx_desc __attribute__((aligned(64))) = {0};
union ixgbe_adv_tx_desc ixgbe_adv_tx_desc __attribute__((aligned(64))) = {0};

int main(const int argc, char** argv) {
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

  u32 i = ixgbe_read_reg(&ixgbe_adapter, IXGBE_RDH);
  while(1){
    barrier();
    if(likely(rx_ring[i].wb.status_error & IXGBE_RXD_STAT_DD)){
      rmb();
      batch_manage_tail_counter++;
      total_packets++;
      /* This PoC only includes ICMP. */
      if(unlikely(rx_ring[i].wb.pkt_info != 0)){
        irrelevant_packets++;
        break;
      } 
      /* Packet parsing logic is added temporarily to prove pointer arithmatics on structures.
      * Since the driver cannot reply ARP's, static ARP configuration needed.
      */
      u8* pkt = (u8*)ixgbe_adapter.rx_base + (256 * 1024) + ( i * 2048);
      struct eth_hdr *eth = (struct eth_hdr *)pkt;
      u8* src_mac = eth->src_mac;
      printf("source mac address: %0x:%0x:%0x:%0x:%0x:%0x\n", src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
      u8* dst_mac = eth->dst_mac;
      printf("destination mac address: %0x:%0x:%0x:%0x:%0x:%0x\n", dst_mac[0], dst_mac[1], dst_mac[2], dst_mac[3], dst_mac[4], dst_mac[5]);
      struct ip_hdr *ip = (struct ip_hdr *)(pkt + sizeof(struct eth_hdr));
      u32 src_ip = __builtin_bswap32(ip->src_addr); /* See little endian/big endian byte orders.*/
      printf("source ip addrress: %0x\n", src_ip);
      u32 dst_ip = __builtin_bswap32(ip->dst_addr);
      printf("destination ip address: %0x\n",dst_ip);
      if(unlikely(batch_manage_tail_counter == batch_manage_tail)){
        ixgbe_write_reg(&ixgbe_adapter, IXGBE_RDT, i);
        batch_manage_tail_counter = 0;
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
      ixgbe_write_reg(&ixgbe_adapter, IXGBE_TDT, i + 1 );
      /* Reset Descriptor Done */
      rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
      i = IXGBE_BUFFER_ADVANCE(i, 1);
      printf("tail : %u\n", ixgbe_read_reg(&ixgbe_adapter,IXGBE_TDT));
      printf("head : %u\n", ixgbe_read_reg(&ixgbe_adapter,IXGBE_TDH));
  }
  }
}