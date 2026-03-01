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
  while(1){
    barrier();
    if(likely(rx_ring[0].wb.status_error & IXGBE_RXD_STAT_DD)){
      /*
      * Temporary for proving current situation.
      * A packet can be captured with ./driver <pci-addr> > filename, which will be raw hexadecimal.
      * Currently we lack of head-tail management logic, means it'll drop packets after buffers are end.
      * When tested, the captured packet was DHCP request, sent by the second port which was in still kernel's control.
      * This was because global reset also affects it.
      */
      u8* pkt = (u8*)ixgbe_adapter.rx_base + (256 * 1024) + ( 0 * 2048);
      u32 i;
         for (i = 0; i < rx_ring[0].wb.length; i++){
       printf("%02x ", pkt[i]);
         }
               if(rx_ring[0].wb.status_error & IXGBE_RXD_STAT_EOP){
        printf("EOP");
        return 0;
      }
  }
  }
}