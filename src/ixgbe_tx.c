#include "base.h"
#include "hw.h"
#include <asm-generic/errno.h>
#include <errno.h>
#include "ixgbe.h"
#include <unistd.h>
int tx_ring_probe(const struct hw* hw){
  /* divide base addr to low and high */
  u64 addr_base = (uintptr_t)(hw->tx_base_phy);
  u32 addr_low = addr_base & 0xFFFFFFFF;
  u32 addr_high = addr_base >> 32;
  u32 read_val;
  u32 delay;
  /* Halt Tx*/
  read_val = ixgbe_read_reg(hw, IXGBE_DMATXCTL);
  IXGBE_CLEAR_BITS(read_val, IXGBE_DMATXCTL_TE);
  ixgbe_write_reg(hw, IXGBE_DMATXCTL, read_val);
  read_val = ixgbe_read_reg(hw, IXGBE_RTTDCS);
  IXGBE_SET_BITS(read_val, IXGBE_RTTDCS_ARBDIS);
  ixgbe_write_reg(hw, IXGBE_RTTDCS, read_val);
  /* Configure pcie req limit to max */
  read_val = ixgbe_read_reg(hw, IXGBE_DTXMXSZRQ);
  IXGBE_SET_BITS(read_val, IXGBE_DTXMXSZRQ_MAX_REQ);
  ixgbe_write_reg(hw, IXGBE_DTXMXSZRQ, read_val);
  /* Configure thresholds */
  /* Note that this threshold configurations has high overhead on PCI bus.
   * Should not be used but ultra low latency workloads.
   */
  read_val = ixgbe_read_reg(hw, IXGBE_TXDCTL);
  IXGBE_SET_BITS(read_val, IXGBE_TXDCTL_PTHRESH);
  IXGBE_CLEAR_BITS(read_val, IXGBE_TXDCTL_WTHRESH);
  IXGBE_CLEAR_BITS(read_val, IXGBE_TXDCTL_HTHRESH);
  ixgbe_write_reg(hw, IXGBE_TXDCTL, read_val);
  read_val = ixgbe_read_reg(hw, IXGBE_RTTDCS);
  IXGBE_CLEAR_BITS(read_val, IXGBE_RTTDCS_ARBDIS);
  ixgbe_write_reg(hw, IXGBE_RTTDCS, read_val);
  /* write memory addresses */
  ixgbe_write_reg(hw, IXGBE_TDBAL, addr_low);
  ixgbe_write_reg(hw, IXGBE_TDBAH, addr_high);
  ixgbe_write_reg(hw,IXGBE_TDLEN,TDLEN_VAL);
  /* reset head and tail */
  ixgbe_write_reg(hw, IXGBE_TDH, 0x0);
  ixgbe_write_reg(hw, IXGBE_TDT, 0x0);
  /* Enable Tx */
  read_val = ixgbe_read_reg(hw, IXGBE_DMATXCTL);
  IXGBE_SET_BITS(read_val, IXGBE_DMATXCTL_TE);
  ixgbe_write_reg(hw, IXGBE_DMATXCTL, read_val);
  read_val = ixgbe_read_reg(hw, IXGBE_TXDCTL);
  IXGBE_SET_BITS(read_val, IXGBE_TXDCTL_TX_EN);
  ixgbe_write_reg(hw, IXGBE_TXDCTL, read_val);
  delay = 10;
  for (u8 i = 0; i < 15; i++) {
    u32 err = ixgbe_read_reg(hw, IXGBE_TXDCTL);
    if (likely(IXGBE_IS_SET(err, IXGBE_TXDCTL_TX_EN))) goto tx_ready;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  return -ETIMEDOUT;
  tx_ready:;
  return 0;
}

  /* According to errata 13, rev 4.3.3,
   * Changes in the internal link-speed might hang transmit.
   * This function issues the workaround specified. Should be called if
   * Tx cannot be initialized.
  */
void clock_switching_workaround(const struct hw* hw){
  u32 read_val = ixgbe_read_reg(hw, IXGBE_AUTOC2);
  /* 19th bit is reserved ( undocumented ) in datasheet, but errata says
  * it delays link-up flow by 10 microseconds. Probably a register used to be internal,
  * but published in errata...
  */
  IXGBE_SET_BITS(read_val,(1 << 19));
  ixgbe_write_reg(hw, IXGBE_AUTOC2, read_val);
}