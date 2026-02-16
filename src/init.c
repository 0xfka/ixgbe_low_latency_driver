#include <errno.h>
#include <unistd.h>

#include "base.h"
#include "hw.h"
#include "ixgbe.h"
/*
 * Initialize ASIC following 82599 datasheet page 166.
 */
int ixgbe_probe(const struct hw* hw) {
  u32 delay = 10;
  /* Disable Interrupts */
  ixgbe_write_reg(hw, IXGBE_EIMC, 0x7FFFFFFF);
  u32 err = ixgbe_read_reg(hw, IXGBE_EIMS);
  if (unlikely(err != 0)) return -EIO;
  /* Global Reset */
  u32 ctrl = ixgbe_read_reg(hw, IXGBE_CTRL);
  IXGBE_SET_BITS(ctrl, IXGBE_CTRL_RST | IXGBE_CTRL_LRST);
  ixgbe_write_reg(hw, IXGBE_CTRL, ctrl);
  /* ~10.23 ms at total. +~10ms means malfunctional behavior. */
  delay = 10;
  for (u8 i = 0; i < 15; i++) {
    err = ixgbe_read_reg(hw, IXGBE_CTRL);
    if (likely(IXGBE_IS_CLEAR(err, (IXGBE_CTRL_RST | IXGBE_CTRL_LRST)))) break;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  if (unlikely(IXGBE_IS_SET(err, (IXGBE_CTRL_RST | IXGBE_CTRL_LRST))))
    return -ETIMEDOUT;
  /* Reset delay value since it'll continue from Global Reset. */
  delay = 10;
  for (u8 i = 0; i < 23; i++) {
    const u32 eeprom = ixgbe_read_reg(hw, IXGBE_EEC);
    if (IXGBE_IS_SET(eeprom,IXGBE_EEC_ARD)) goto eeprom_ok;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  return -ENODEV;
eeprom_ok:
  /* ~20.47 ms at total. +~20ms means malfunctional behavior. */
  delay = 10;
  for (u8 i = 0; i < 23; i++) {
    const u32 dmaidone = ixgbe_read_reg(hw, IXGBE_RDRXCTL);
    if (IXGBE_IS_SET(dmaidone,IXGBE_RDRXCTL_DMAIDONE)) goto dmaiok;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  return -ETIMEDOUT;
dmaiok:;
  err = semaphore_acquire(hw, SW_PHY_SM0);
  if (unlikely(err != 0)) return (int)err;
  u32 autoc = ixgbe_read_reg(hw, IXGBE_AUTOC);
  if (unlikely(autoc == 0xFFFFFFFF)) return -ENODEV;
  IXGBE_CLEAR_BITS(autoc, IXGBE_AUTOC_LMS_MASK);
  IXGBE_SET_BITS(autoc, 0x6 << IXGBE_AUTOC_LMS_SHIFT);
  ixgbe_write_reg(hw, IXGBE_AUTOC, autoc);
  err = ixgbe_read_reg(hw, IXGBE_AUTOC);
  if (unlikely((err & IXGBE_AUTOC_LMS_MASK) != (autoc & IXGBE_AUTOC_LMS_MASK)))
    return -EIO;
  err = semaphore_release(hw, SW_PHY_SM0);
  if (unlikely(err != 0)) return (int)err;
  /* Will be continued from sixth step of title '4.6.3 Initialization Sequence'.
   */
  return 0;
}
/*
 * Acquire semaphore of `ixge_swfw_sync_t` parameter.
 * acquire parameter must be software owned parameter in SMBITS SW_FW_SYNC,
 * title 8.2.3.4.11.
 */
int semaphore_acquire(const struct hw* hw, const ixgbe_swfw_sync_t acquire) {
  bool fw_malfunction = false;
  bool sw_malfunction = false;
  u32 reset_sw_bits;
  u32 read_val;
  u32 sync_val;
  u32 sw_bits;
  u32 delay = 10;
  u8 count = 0;
  u8 count_crit = 0;
  constexpr u8 max_retr = 100;
  constexpr u8 max_retr_crit = 3;
semaphore_main:
  /* ~19 ms at total. +~10ms means malfunctional behavior from Software. */
  delay = 10;
  for (u8 i = 0; i < 25; i++) {
    const u32 swsm = ixgbe_read_reg(hw, IXGBE_SWSM);
    if (IXGBE_IS_CLEAR(swsm,IXGBE_SWSM_SMBI)) goto semaphore_free;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  sw_malfunction = true;
semaphore_free:
  u32 semaphore = ixgbe_read_reg(hw, IXGBE_SWSM);
  IXGBE_SET_BITS(semaphore, IXGBE_SWSM_SWESMBI);
  ixgbe_write_reg(hw, IXGBE_SWSM, semaphore);
  IXGBE_WRITE_FLUSH(hw);
  /* ~3.76 s at total. +~3s means malfunctional behavior from Firmware. */
  delay = 10;
  for (u8 i = 0; i < 50; i++) {
    const u32 swsm = ixgbe_read_reg(hw, IXGBE_SWSM);
    if (IXGBE_IS_SET(swsm, IXGBE_SWSM_SWESMBI)) goto read_sw_fw_sync;
    usleep(delay);
    if (likely(delay < 100000)) delay *= 2;
  }
  fw_malfunction = true;
read_sw_fw_sync:
  if (sw_malfunction) {
    /* Clear unowned sw bits */
    sw_bits = (IXGBE_SWFW_EEP_SM | IXGBE_SWFW_FLASH_SM | IXGBE_SWFW_MAC_CSR_SM |
               IXGBE_SWFW_PHY0_SM | IXGBE_SWFW_PHY1_SM);
    read_val = ixgbe_read_reg(hw, IXGBE_SW_FW_SYNC);
    IXGBE_CLEAR_BITS(read_val, sw_bits & ~acquire);
    ixgbe_write_reg(hw, IXGBE_SW_FW_SYNC, read_val);
    IXGBE_WRITE_FLUSH(hw);
  }
  if (fw_malfunction) {
    /* Clear all fw bits */
    read_val = ixgbe_read_reg(hw, IXGBE_SW_FW_SYNC);
    IXGBE_CLEAR_BITS(read_val, IXGBE_FWFW_EEP_SM | IXGBE_FWFW_FLASH_SM |
                                   IXGBE_FWFW_MAC_CSR_SM | IXGBE_FWFW_PHY0_SM |
                                   IXGBE_FWFW_PHY1_SM);
    ixgbe_write_reg(hw, IXGBE_SW_FW_SYNC, read_val);
    IXGBE_WRITE_FLUSH(hw);
  }
  const u32 mask = acquire | IXGBE_SWFW_TO_FW_MASK(acquire);
  read_val = ixgbe_read_reg(hw, IXGBE_SW_FW_SYNC);
  if (unlikely(IXGBE_IS_CLEAR(read_val, mask))) goto accesible;
  usleep(10000);
  goto retr;
accesible:
  sync_val = ixgbe_read_reg(hw, IXGBE_SW_FW_SYNC);
  IXGBE_SET_BITS(sync_val, acquire);
  ixgbe_write_reg(hw, IXGBE_SW_FW_SYNC, sync_val);
  reset_sw_bits = ixgbe_read_reg(hw, IXGBE_SWSM);
  IXGBE_CLEAR_BITS(reset_sw_bits, IXGBE_SWSM_SMBI | IXGBE_SWSM_SWESMBI);
  ixgbe_write_reg(hw, IXGBE_SWSM, reset_sw_bits);
  IXGBE_WRITE_FLUSH(hw);
  return 0;
retr:
  count++;
  if (unlikely(count == max_retr)) goto clear_retr;
  reset_sw_bits = ixgbe_read_reg(hw, IXGBE_SWSM);
  IXGBE_CLEAR_BITS(reset_sw_bits, IXGBE_SWSM_SMBI | IXGBE_SWSM_SWESMBI);
  ixgbe_write_reg(hw, IXGBE_SWSM, reset_sw_bits);
  IXGBE_WRITE_FLUSH(hw);
  goto semaphore_main;

clear_retr:
  if (unlikely(count_crit == max_retr_crit)) return -EDEADLK;
  sw_bits = (IXGBE_SWFW_EEP_SM | IXGBE_SWFW_FLASH_SM | IXGBE_SWFW_MAC_CSR_SM |
             IXGBE_SWFW_PHY0_SM | IXGBE_SWFW_PHY1_SM);
  read_val = ixgbe_read_reg(hw, IXGBE_SW_FW_SYNC);
  IXGBE_CLEAR_BITS(read_val, sw_bits & ~acquire);
  ixgbe_write_reg(hw, IXGBE_SW_FW_SYNC, read_val);
  IXGBE_WRITE_FLUSH(hw);
  reset_sw_bits = ixgbe_read_reg(hw, IXGBE_SWSM);
  IXGBE_CLEAR_BITS(reset_sw_bits, IXGBE_SWSM_SMBI | IXGBE_SWSM_SWESMBI);
  ixgbe_write_reg(hw, IXGBE_SWSM, reset_sw_bits);
  IXGBE_WRITE_FLUSH(hw);
  count = 0;
  count_crit++;
  goto semaphore_main;
}
int semaphore_release(const struct hw* hw, const ixgbe_swfw_sync_t acquire) {
  u32 release_source = ixgbe_read_reg(hw, IXGBE_SW_FW_SYNC);
  IXGBE_CLEAR_BITS(release_source, acquire);
  ixgbe_write_reg(hw, IXGBE_SW_FW_SYNC, release_source);
  u32 release_semaphore = ixgbe_read_reg(hw, IXGBE_SWSM);
  IXGBE_CLEAR_BITS(release_semaphore, IXGBE_SWSM_SMBI | IXGBE_SWSM_SWESMBI);
  ixgbe_write_reg(hw, IXGBE_SWSM, release_semaphore);
  IXGBE_WRITE_FLUSH(hw);
  usleep(10000);
  return 0;
}