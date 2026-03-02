#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "../selftests/selftests.h"
#include "base.h"
#include "hw.h"
#include "ixgbe.h"
#include "pci.h"
struct hw ixgbe_adapter __attribute__((aligned(64))) = {0};
union ixgbe_adv_rx_desc ixgbe_adv_rx_desc __attribute__((aligned(64))) = {0};

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
  err = virt2phy(&ixgbe_adapter);
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
  u32 print_counter = 0;

  u32 i = ixgbe_read_reg(&ixgbe_adapter, IXGBE_RDH);
  while(1){
    barrier();
    if(likely(rx_ring[i].wb.status_error & IXGBE_RXD_STAT_DD)){
      rmb();
      batch_manage_tail_counter++;
      total_packets++;
      /* Disabled for testing ring buffer behaviour */
      /* This PoC only includes ICMP. */
      /* if(unlikely(rx_ring[i].wb.pkt_info != 0)){
        break;
      } */
      u8* pkt = (u8*)ixgbe_adapter.rx_base + (256 * 1024) + ( i * 2048);
      if(unlikely(total_packets == 256)){
        printf("256 packet, %x\n", print_counter);
        print_counter++;
        total_packets = 0;
      }
      if(unlikely(batch_manage_tail_counter == batch_manage_tail)){
        ixgbe_write_reg(&ixgbe_adapter, IXGBE_RDT, i);
        batch_manage_tail_counter = 0;
      }
      /* Reset Descriptor Done */
      rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
      i = IXGBE_BUFFER_ADVANCE(i, 1);
  }
  }
}