#include <errno.h>
#include <unistd.h>

#include "base.h"
#include "hw.h"
#include "ixgbe.h"
/*
 * Initialize ASIC following 82599 datasheet page 166.
 */
int ixgbe_probe(const struct hw* hw) {
  u32 count_crit = 0;
  u32 max_retr_crit = 3;
  u32 delay = 10;
  /* Disable Interrupts */
  ixgbe_write_reg(hw, IXGBE_EIMC, 0x7FFFFFFF);
  u32 err = ixgbe_read_reg(hw, IXGBE_EIMS);
  if (unlikely(err != 0)) return -EIO;
  master_disable:;
  u32 ctrl = ixgbe_read_reg(hw,IXGBE_CTRL);
  IXGBE_SET_BITS(ctrl,IXGBE_CTRL_MASTER);
  ixgbe_write_reg(hw,IXGBE_CTRL,ctrl);
  /* Datasheet doesn't specifies a timeout for master disable. */
  delay = 10;
  for (u16 i = 0; i < 788; i++) {
  ctrl = ixgbe_read_reg(hw,IXGBE_STATUS);
  if (likely(IXGBE_IS_CLEAR(ctrl, IXGBE_STATUS_MASTER))) goto global_reset;
      usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  if (unlikely(count_crit == max_retr_crit)) return -EDEADLK;
  count_crit++;
  master_disable_workaround(hw);
  goto master_disable;
  global_reset:;
  /* Global Reset */
  ctrl = ixgbe_read_reg(hw, IXGBE_CTRL);
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
    if (IXGBE_IS_SET(eeprom, IXGBE_EEC_ARD)) goto eeprom_ok;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  return -ENODEV;
eeprom_ok:
  /* ~20.47 ms at total. +~20ms means malfunctional behavior. */
  delay = 10;
  for (u8 i = 0; i < 23; i++) {
    const u32 dmaidone = ixgbe_read_reg(hw, IXGBE_RDRXCTL);
    if (IXGBE_IS_SET(dmaidone, IXGBE_RDRXCTL_DMAIDONE)) goto dmaiok;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  return -ETIMEDOUT;
dmaiok:;
  /* fifth step was skipped. See commit logs.*/
  /* As sixth step, 'Initialize all statistical counters', only GPRC, GPTC,
   * TPR and TPT will be observed. Read operation provides resetting.
   * This registers will be read from another core to debug receive/transmit
   * steps.
   */
  /* Update: According to Specification Update july 2024 errata 7,
   * GPRC and GORCL/H also counts missed packets. While GPRC is decided for
   * future observing implementation. Solutions are;
   * Reading MPC and subtracting or using QPRC.
   * Note that GORCL/H is already not going to be used.
   */
  (void)ixgbe_read_reg(hw, IXGBE_GPRC);
  (void)ixgbe_read_reg(hw, IXGBE_GPTC);
  (void)ixgbe_read_reg(hw, IXGBE_TPR);
  (void)ixgbe_read_reg(hw, IXGBE_TPT);
  /* 7. Initialize receive (see Section 4.6.7). */
  /* Provides mac addresses for VM's, will be cleared from 1-127. */
  for (u8 i = 1; i < 128; i++) {
    u32 read_val = ixgbe_read_reg(hw, IXGBE_RAH + i * 8);
    IXGBE_CLEAR_BITS(read_val, IXGBE_RAH_AV);
    ixgbe_write_reg(hw, IXGBE_RAH + i * 8, read_val);
  }
  for (u8 i = 0; i < 128; i++) {
    ixgbe_write_reg(hw, IXGBE_PFUTA + i * 4, 0x0);
  }
  /* Vlan's will not be used in this PoC. */
  for (u8 i = 0; i < 128; i++) {
    ixgbe_write_reg(hw, IXGBE_VFTA + i * 4, 0x0);
  }
  for (u8 i = 0; i < 64; i++) {
    ixgbe_write_reg(hw, IXGBE_PFVLVF + i * 4, 0x0);
  }
  for (u16 i = 1; i < 256; i++) {
    ixgbe_write_reg(hw, IXGBE_MPSAR + i * 4, 0x0);
  }
  for (u8 i = 0; i < 128; i++) {
    ixgbe_write_reg(hw, IXGBE_PFVLVFB + i * 4, 0x0);
  }
  /* Disable error correcting debugging.
   * Provides ECC for the Rx filter memory that is disabled.
   * Doesn't affect the functionality of correcting 1 bit flip,
   * while enabling it may stop Rx on 2-3 bit errors.
   */
  u32 read_val = ixgbe_read_reg(hw, IXGBE_RXFECCERR0);
  IXGBE_CLEAR_BITS(read_val, IXGBE_RXFECCERR0_ECCFLT_EN);
  ixgbe_write_reg(hw, IXGBE_RXFECCERR0, read_val);
  /* Rx offloads */
  /* Filter Control Registers */
  /* Broadcast was cleared since the ARP will be hardcoded. */
  read_val = ixgbe_read_reg(hw, IXGBE_FCTRL);
  IXGBE_SET_BITS(read_val, IXGBE_FCTRL_BAM);
  IXGBE_CLEAR_BITS(read_val, IXGBE_FCTRL_MPE);
  IXGBE_CLEAR_BITS(read_val, IXGBE_FCTRL_SBP);
  IXGBE_SET_BITS(read_val, IXGBE_FCTRL_UPE);
  ixgbe_write_reg(hw,IXGBE_FCTRL,read_val);
  /* Out of scope, disabled. */
  read_val = ixgbe_read_reg(hw, IXGBE_VLNCTRL);
  IXGBE_CLEAR_BITS(read_val, IXGBE_VLNCTRL_VFE);
  ixgbe_write_reg(hw,IXGBE_VLNCTRL,read_val);
  read_val = ixgbe_read_reg(hw,IXGBE_MCSTCTRL);
  IXGBE_CLEAR_BITS(read_val,IXGBE_MCSTCTRL_MFE);
  ixgbe_write_reg(hw,IXGBE_MCSTCTRL,read_val);
  /*  
   * According to errata 44, spec update rev 4.3.3,   
   * header splitting should not be enabled.
   */
  read_val = ixgbe_read_reg(hw, IXGBE_PSRTYPE);
  IXGBE_CLEAR_BITS(read_val, IXGBE_PSRTYPE_SPLIT_MASK);
  ixgbe_write_reg(hw, IXGBE_PSRTYPE, read_val);
  /*
   * There's erratas that include unstable behavior 
   * on checksums (43, 60 ,66) . As decided, 
   * RFC 1624 will be used. 
    */
  read_val = ixgbe_read_reg(hw,IXGBE_RXCSUM);
  IXGBE_CLEAR_BITS(read_val,IXGBE_RXCSUM_IPPCSE);
  IXGBE_CLEAR_BITS(read_val,IXGBE_RXCSUM_PCSD);
  ixgbe_write_reg(hw,IXGBE_RXCSUM,read_val);
  /* 1 core polling data path architecture invalids RSS usage.*/
  ixgbe_write_reg(hw, IXGBE_RQTC, 0x0);
  /**/
  read_val = ixgbe_read_reg(hw,IXGBE_RFCTL);
  /* Disabled, may add latency to buffer packets. 
   * Also, may cause TLP overhead on PCI but goal is lowest latency.*/
  IXGBE_SET_BITS(read_val, IXGBE_RFCTL_RSC_DIS);
  /* Disabled to prevent latency emerging from ASIC checking 
   * the packet is NFS or not. */
  IXGBE_SET_BITS(read_val, IXGBE_RFCTL_NFSW_DIS);
  IXGBE_SET_BITS(read_val, IXGBE_RFCTL_NFSR_DIS);
  /* IPv6 disable and IP Fragment Split Disable
   *  will not set of 'for internal usage' warning. */
  ixgbe_write_reg(hw,IXGBE_RFCTL,read_val);
  /* Match PF pool with mac addr */
  read_val = ixgbe_read_reg(hw, IXGBE_MPSAR);
  IXGBE_SET_BITS(read_val, (1 << 0));
  ixgbe_write_reg(hw, IXGBE_MPSAR, read_val);
  /* Set receive buffer sizes */
  read_val = ixgbe_read_reg(hw,IXGBE_SRRCTL);
  IXGBE_CLEAR_BITS(read_val, IXGBE_SRRCTL_BSIZEPACKET);
  IXGBE_SET_BITS(read_val, 1 << 1);
  IXGBE_CLEAR_BITS(read_val, IXGBE_SRRCTL_DESCTYPE);
  IXGBE_SET_BITS(read_val, 1 << 25);
  ixgbe_write_reg(hw, IXGBE_SRRCTL, read_val);
  err = rx_ring_probe(hw);
  if (unlikely(err != 0)) return err;
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
  const u8 max_retr = 100;
  const u8 max_retr_crit = 3;
semaphore_main:
    /* Reset malfunction flags to prevent entering workaround unnecessarily*/
     fw_malfunction = false;
     sw_malfunction = false;
  /* ~19 ms at total. +~10ms means malfunctional behavior from Software. */
  delay = 10;
  for (u8 i = 0; i < 25; i++) {
    const u32 swsm = ixgbe_read_reg(hw, IXGBE_SWSM);
    if (IXGBE_IS_CLEAR(swsm, IXGBE_SWSM_SMBI)) goto semaphore_free;
    usleep(delay);
    if (likely(delay < 1000)) delay *= 2;
  }
  sw_malfunction = true;
semaphore_free:;
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
  if (likely(IXGBE_IS_CLEAR(read_val, mask))) goto accesible;
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
  /* According to errata 9, rev 4.3.3, 
   * there are cases where PCIe Master Enable bit
   * is not released. This function issues a Master Disable
   * and the recommendations specified.
  */
void master_disable_workaround(const struct hw* hw){
  /* Flush data path first */
  u32 hlreg0 = ixgbe_read_reg(hw,IXGBE_HLREG0);
  IXGBE_SET_BITS(hlreg0,IXGBE_HLREG0_LPBK);
  ixgbe_write_reg(hw,IXGBE_HLREG0,hlreg0);
  u32 rxctrl = ixgbe_read_reg(hw, IXGBE_RXCTRL);
  IXGBE_CLEAR_BITS(rxctrl, IXGBE_RXCTRL_RXEN);
  ixgbe_write_reg(hw,IXGBE_RXCTRL,rxctrl);
  u32 gcr_ext = ixgbe_read_reg(hw,IXGBE_GCR_EXT);
  IXGBE_SET_BITS(gcr_ext, IXGBE_GCR_EXT_BUFFERS_CLEAR_FUNC);
  ixgbe_write_reg(hw,IXGBE_GCR_EXT,gcr_ext);
  usleep(20);
  IXGBE_CLEAR_BITS(hlreg0, IXGBE_HLREG0_LPBK);
  IXGBE_CLEAR_BITS(gcr_ext, IXGBE_GCR_EXT_BUFFERS_CLEAR_FUNC);
  ixgbe_write_reg(hw, IXGBE_GCR_EXT, gcr_ext);
  ixgbe_write_reg(hw,IXGBE_HLREG0,hlreg0);
  u32 ctrl = ixgbe_read_reg(hw, IXGBE_CTRL);
  IXGBE_SET_BITS(ctrl,IXGBE_CTRL_RST);
  ixgbe_write_reg(hw, IXGBE_CTRL, ctrl);
  /* Larger than 1 microsecond is recommended */
  usleep(10);
  /* Two consecutive software resets are recommended */
  ixgbe_write_reg(hw, IXGBE_CTRL, ctrl);
}