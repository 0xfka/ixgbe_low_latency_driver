#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "hw.h"
#include "ixgbe.h"
/* Preparing Rx to get started with a single RDT write. */
int rx_ring_probe(const struct hw* hw) {
  /* divide base addr to low and high */
  u64 addr_base = (uintptr_t)(hw->rx_base_phy);
  u32 addr_low = addr_base & 0xFFFFFFFF;
  u32 addr_high = addr_base >> 32;
  u32 read_val;
  u32 delay;
  /* Halt Rx */
  read_val = ixgbe_read_reg(hw, IXGBE_SECRXCTRL);
  IXGBE_SET_BITS(read_val, IXGBE_SECRXCTRL_RX_DIS);
  ixgbe_write_reg(hw, IXGBE_SECRXCTRL, read_val);
  read_val = ixgbe_read_reg(hw, IXGBE_CTRL_EXT);
  IXGBE_SET_BITS(read_val, IXGBE_CTRL_EXT_NS_DIS);
  ixgbe_write_reg(hw, IXGBE_CTRL_EXT, read_val);
  read_val = ixgbe_read_reg(hw, IXGBE_DCA_RXCTRL);
  IXGBE_CLEAR_BITS(read_val, IXGBE_DCA_RXCTRL_SET_TO_0);
  ixgbe_write_reg(hw, IXGBE_DCA_RXCTRL, read_val);
  ixgbe_write_reg(hw, IXGBE_RDBAL, addr_low);
  ixgbe_write_reg(hw, IXGBE_RDBAH, addr_high);
  ixgbe_write_reg(hw, IXGBE_RDLEN, RDLEN_VAL);
  read_val = ixgbe_read_reg(hw, IXGBE_RXDCTL);
  IXGBE_SET_BITS(read_val, IXGBE_RXDCTL_RX_EN);
  ixgbe_write_reg(hw, IXGBE_RXDCTL, read_val);
  delay = 10;
  for (u8 i = 0; i < 15; i++) {
    u32 err = ixgbe_read_reg(hw, IXGBE_RXDCTL);
    if (likely(IXGBE_IS_SET(err, IXGBE_RXDCTL_RX_EN))) goto rx_ready;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  return -ETIMEDOUT;
rx_ready:
  /* Release Rx */
  read_val = ixgbe_read_reg(hw, IXGBE_SECRXCTRL);
  IXGBE_CLEAR_BITS(read_val, IXGBE_SECRXCTRL_RX_DIS);
  ixgbe_write_reg(hw, IXGBE_SECRXCTRL, read_val);
  ixgbe_write_reg(hw, IXGBE_RXCTRL, IXGBE_RXCTRL_RXEN );
  u64 buffer_base_phy = hw->rx_base_phy + (256 * 1024);
  union ixgbe_adv_rx_desc *rx_ring = (union ixgbe_adv_rx_desc *)hw->rx_base;
  for (u16 i = 0; i < BUFFER_NUMBER; i++){
    rx_ring[i].read.pkt_addr = buffer_base_phy + (i * 2048);
    rx_ring[i].read.hdr_addr = 0;
  }
  /* Writing RDT will start Rx */
  return 0;
}