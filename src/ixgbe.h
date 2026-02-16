#ifndef IXGBE_H
#define IXGBE_H

#include "base.h"
#include "hw.h"

#define IXGBE_SET_BITS(val, bits) ((val) |= (bits))
#define IXGBE_CLEAR_BITS(val, bits) ((val) &= ~(bits))
#define IXGBE_IS_SET(offset, mask)   (!!((offset) & (mask)))
#define IXGBE_IS_CLEAR(offset, mask) (!((offset) & (mask)))
/* Registers needed for 4.6.3 Initialization Sequence */

/* Device Control Register */
#define IXGBE_CTRL 0x00000
#define IXGBE_CTRL_RST (1 << 26)
#define IXGBE_CTRL_LRST (1 << 3)

/* Device Status Register */
#define IXGBE_STATUS 0x00008

/* Extended Device Control Register */
#define IXGBE_CTRL_EXT 0x00018

/* Extended Interrupt Cause Register */
#define IXGBE_EICR 0x00800

/* Extended Interrupt Cause Set Register */
#define IXGBE_EICS 0x00808

/* Extended Interrupt Auto Clear Register */
#define IXGBE_EIAC 0x00810

/* Extended Interrupt Mask Set/Read Register */
#define IXGBE_EIMS 0x00880

/* Extended Interrupt Mask Clear Register */
#define IXGBE_EIMC 0x00888

/* General Purpose Interrupt Enable */
#define IXGBE_GPIE 0x00898

/* Interrupt Vector Allocation Registers */
#define IXGBE_IVAR 0x00900

/* EEPROM/Flash Control Register */
#define IXGBE_EEC 0x10010
#define IXGBE_EEC_ARD (1 << 9) /* Auto Read Done */
/* EEPROM Read Register */
#define IXGBE_EERD 0x10014

/* Receive DMA Control Register */
#define IXGBE_RDRXCTL 0x02F00
#define IXGBE_RDRXCTL_DMAIDONE (1 << 3)

/* PF Queue Drop Enable Register */
#define IXGBE_PFQDE 0x02F04
/* LED Control Register */
#define IXGBE_LEDCTL 0x00200
/* Global Blink Rate 0=200ms, 1=83ms */
#define IXGBE_LED_GLOBAL_FAST (1 << 5)
/* Base Bit Positions (For shifting on LED0, LED1 etc. ) */
#define IXGBE_LED_IVRT (1 << 6)  /* Invert Polarity  */
#define IXGBE_LED_BLINK (1 << 7) /* Blink Enable [ */
#define IXGBE_LED_MODE_MASK 0xF  /* Mode is bits 3:0 */
/* Manual Modes  */
#define IXGBE_LED_MODE_ON 0xE  /* Always On/Blink */
#define IXGBE_LED_MODE_OFF 0xF /* Always Off */
/* Reserved Bits */
#define IXGBE_LED_RW_MASK 0xCFCFCFEF

/* Macro to position settings for a specific LED (0-3) */
#define IXGBE_LED_CONF(idx, val) ((val) << ((idx) * 8))

/* Receive Control Register */
#define IXGBE_RXCTRL 0x03000

/* RSC Data Buffer Control Register */
#define IXGBE_RSCDBU 0x03028

/* Flow Control Transmit Timer Value */
#define IXGBE_FCTTV 0x03200

/* Flow Control Receive Threshold Low */
#define IXGBE_FCRTL 0x03220

/* Flow Control Receive Threshold High */
#define IXGBE_FCRTH 0x03260

/* Flow Control Refresh Threshold Value */
#define IXGBE_FCRTV 0x032A0

/* Receive Packet Buffer Size */
#define IXGBE_RXPBSIZE 0x03C00

/* Flow Control Configuration */
#define IXGBE_FCCFG 0x03D00

/* MAC Core Control 0 Register */
#define IXGBE_HLREG0 0x04240

/* Max Frame Size */
#define IXGBE_MAXFRS 0x04268

/* MAC Flow Control Register */
#define IXGBE_MFLCN 0x04294

/* Auto Negotiation Control Register */
#define IXGBE_AUTOC 0x042A0
/* Link Mode Select */
#define IXGBE_AUTOC_LMS_SHIFT 13
#define IXGBE_AUTOC_LMS_MASK (0x7 << IXGBE_AUTOC_LMS_SHIFT)

/* Link Status Register */
#define IXGBE_LINKS 0x042A4

/* Auto Negotiation Control 2 Register */
#define IXGBE_AUTOC2 0x042A8

/* DMA Tx Control */
#define IXGBE_DMATXCTL 0x04A80

/* Receive Checksum Control */
#define IXGBE_RXCSUM 0x05000

/* Receive Filter Control Register */
#define IXGBE_RFCTL 0x05008

/* Multicast Table Array */
#define IXGBE_MTA 0x05200

/* Filter Control Register */
#define IXGBE_FCTRL 0x05080

/* VLAN Control Register */
#define IXGBE_VLNCTRL 0x05088

/* Multicast Control Register */
#define IXGBE_MCSTCTRL 0x05090

/* EType Queue Filter */
#define IXGBE_ETQF 0x05128

/* Rx Filter ECC Err Insertion 0 */
#define IXGBE_RXFECCERR0 0x051B8

/* VT Control Register */
#define IXGBE_PFVTCTL 0x051B0

/* Flexible Host Filter Table Registers */
#define IXGBE_FHFT 0x09000

/* VLAN Filter Table Array */
#define IXGBE_VFTA 0x0A000

/* Receive Address Low */
#define IXGBE_RAL 0x0A200

/* Receive Address High */
#define IXGBE_RAH 0x0A204

/* MAC Pool Select Array */
#define IXGBE_MPSAR 0x0A600

/* Receive Descriptor Base Address Low */
#define IXGBE_RDBAL 0x01000

/* Receive Descriptor Length */
#define IXGBE_RDLEN 0x01008

/* Receive Descriptor Head */
#define IXGBE_RDH 0x01010

/* Receive Descriptor Tail */
#define IXGBE_RDT 0x01018

/* Receive Descriptor Control */
#define IXGBE_RXDCTL 0x01028

/* Split Receive Control Registers */
#define IXGBE_SRRCTL 0x01014

/* Redirection Table */
#define IXGBE_RETA 0x0EB00

/* RSS Random Key Register */
#define IXGBE_RSSRK 0x0EB80

/* Source Address Queue Filter */
#define IXGBE_SAQF 0x0E000

/* Destination Address Queue Filter */
#define IXGBE_DAQF 0x0E200

/* Five tuple Queue Filter */
#define IXGBE_FTQF 0x0E600

/* SYN Packet Queue Filter */
#define IXGBE_SYNQF 0x0EC30

/* EType Queue Select */
#define IXGBE_ETQS 0x0EC00

/* Packet Split Receive Type Register */
#define IXGBE_PSRTYPE 0x0EA00

/* Receive RSC Control */
#define IXGBE_RSCCTL 0x0102C

/* PF Unicast Table Array */
#define IXGBE_PFUTA 0x0F400

/* PF VM VLAN Pool Filter */
#define IXGBE_PFVLVF 0x0F100

/* PF VM VLAN Pool Filter Bitmap */
#define IXGBE_PFVLVFB 0x0F200

/* Security Rx Control */
#define IXGBE_SECRXCTRL 0x08D00

/* Security Rx Status */
#define IXGBE_SECRXSTAT 0x08D04

/* DCA Rx Control Register */
#define IXGBE_DCA_RXCTRL 0x0100C

/* Semaphore Register */
#define IXGBE_SWSM 0x10140
#define IXGBE_SWSM_SMBI (1 << 0)
#define IXGBE_SWSM_SWESMBI (1 << 1)
#define IXGBE_SWSM_MASK (IXGBE_SWSM_SMBI | IXGBE_SWSM_SWESMBI)

/* Software _ Firmware Synchronization */
#define IXGBE_SW_FW_SYNC 0x10160

#define IXGBE_SWFW_EEP_SM    (1 << 0)
#define IXGBE_SWFW_PHY0_SM   (1 << 1)
#define IXGBE_SWFW_PHY1_SM   (1 << 2)
#define IXGBE_SWFW_MAC_CSR_SM (1 << 3)
#define IXGBE_SWFW_FLASH_SM  (1 << 4)
#define IXGBE_FWFW_EEP_SM    (1 << 5)
#define IXGBE_FWFW_PHY0_SM   (1 << 6)
#define IXGBE_FWFW_PHY1_SM   (1 << 7)
#define IXGBE_FWFW_MAC_CSR_SM (1 << 8)
#define IXGBE_FWFW_FLASH_SM  (1 << 9)

typedef enum {
    SW_EEP_SM     = (1 << 0),
    SW_PHY_SM0    = (1 << 1),
    SW_PHY_SM1    = (1 << 2),
    SW_MAC_CSR_SM = (1 << 3),
    SW_FLASH_SM   = (1 << 4)
} ixgbe_swfw_sync_t;
#define IXGBE_SWFW_TO_FW_MASK(sw_mask) ((sw_mask) << 5)

static inline u32 ixgbe_read_reg(const struct hw* hw, const u32 reg) {
  return *((volatile u32*)(hw->hw_addr + reg));
}

static inline void ixgbe_write_reg(const struct hw* hw, const u32 reg,
                                   const u32 val) {
  *((volatile u32*)(hw->hw_addr + reg)) = val;
}
/* This read operation flushes the PCI write buffer before polling.
* Status offset is selected because it'll not affect firmware state.
* Read operation of some bit offsets may change firmware state machine:
* e.g. Reading semaphore-related bits may processed as semaphore request.
*/
#define IXGBE_WRITE_FLUSH(hw) (void)ixgbe_read_reg((hw), IXGBE_STATUS)

int ixgbe_probe(const struct hw* hw);
int semaphore_acquire(const struct hw* hw,ixgbe_swfw_sync_t);
int semaphore_release(const struct hw* hw,ixgbe_swfw_sync_t);

#endif