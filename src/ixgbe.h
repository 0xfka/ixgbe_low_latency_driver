#ifndef IXGBE_H
#define IXGBE_H

#include "base.h"
#include "hw.h"

#define BUFFER_SIZE 2048 
#define BUFFER_NUMBER 512 /* Must give -1 to NIC for head==tail*/
#define BUFFER_BASE 2 * 128 * 1024 
#define IXGBE_RXD_STAT_DD    0x01  /* Descriptor Done */
#define IXGBE_RXD_STAT_EOP   0x02  /* End of Packet */
#define NUM_DESC     512
#define DESC_SIZE    16
#define RDLEN_VAL    8192
#define TDLEN_VAL    8192
union ixgbe_adv_rx_desc {
struct {
  u64 pkt_addr; /* Packet buffer address */
  u64 hdr_addr; /* Header buffer address */
  } read;
struct {
  u32 rsstype   :4;
  u32 pkttype   :13;
  u32 rsccnt    :4;
  u32 hdr_len   :10;
  u32 sph       :1;
  u32 rss_hash;
  volatile u32 status_error; 
  u16 length;       
  u16 vlan;
  } wb;
};
union ixgbe_adv_tx_desc {
struct {
  u32 iplen           :9;
  u32 maclen          :7;
  u32 vlan            :16;
  u32 ipsec_sa_index  :10;
  u32 fcoef           :6;
  u32 padding         :16;
  u32 ipsec_esp_len   :9;
  u32 tucmd           :11; 
  u32 dtyp            :4;       
  u32 reserved        :1;         
  u32 dext            :1;
  u32 bcntlen         :6;
  u32 idx             :3;
  u32 reserved2       :1;
  u32 l4len           :8;
  u32 mss             :16;     
  u32 padding2        :4;
  } context;
  struct {   
  u64 address     :64;
  u32 dtalen      :16;
  u32 reserved    :2;
  u32 mac         :2;
  u32 dtyp        :4;
  /* DCMD */
  u32 eop         :1;
  u32 ifcs        :1;
  u32 reserved2   :1;
  u32 rs          :1;
  u32 reserved3   :1;
  u32 dext        :1;
  u32 vle         :1;
  u32 tse         :1;
  /* TSA*/
  u32 dd          :1;
  u32 reserved4   :3;
  u32 idx         :3;
  u32 cc          :1;
  /* POPTS*/
  u32 ixsm        :1;
  u32 txsm        :1;
  u32 ipsec       :1;
  u32 reserved5   :3;
  u32 paylen      :18;
    } data_read; 
  struct {
  u64 reserved    :64;
  u32 reserved2   :32;
  u32 sta         :4;
  u32 reserved3   :28;
  } data_wb;
};
struct ixgbe_stats {
  u32 batch_manage_tail;
  u32 batch_manage_tail_counter;
  u32 total_packets;
  u32 irrelevant_packets;
  u32 batch_tx_counter;
  u32 total_bytes_rx;
  u32 total_bytes_tx;
  u32 batch_tx_transmit;
  u32 drop;
};
#define IXGBE_SET_BITS(val, bits) ((val) |= (bits))
#define IXGBE_CLEAR_BITS(val, bits) ((val) &= ~(bits))
#define IXGBE_IS_SET(offset, mask)   (!!((offset) & (mask)))
#define IXGBE_IS_CLEAR(offset, mask) (!((offset) & (mask)))

/* Device Control Register */
#define IXGBE_CTRL 0x00000
#define IXGBE_CTRL_RST (1 << 26)
#define IXGBE_CTRL_LRST (1 << 3)
#define IXGBE_CTRL_MASTER (1 << 2)

/* Device Status Register */
#define IXGBE_STATUS 0x00008
#define IXGBE_STATUS_MASTER (1 << 19)

/* Extended Device Control Register */
#define IXGBE_CTRL_EXT 0x00018
#define IXGBE_CTRL_EXT_NS_DIS (1 << 16)

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
#define IXGBE_RXCTRL_RXEN (1 << 0)

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
#define IXGBE_HLREG0_LPBK (1 << 15)


/* Max Frame Size */
#define IXGBE_MAXFRS 0x04268

/* MAC Flow Control Register */
#define IXGBE_MFLCN 0x04294

/* Auto Negotiation Control Register */
#define IXGBE_AUTOC 0x042A0
/* Restart Auto Negotiation */
#define IXGBE_AUTOC_RESTART 12
/* Link Mode Select */
#define IXGBE_AUTOC_LMS_SHIFT 13
#define IXGBE_AUTOC_LMS_MASK (0x7 << IXGBE_AUTOC_LMS_SHIFT)

/* Link Status Register */
#define IXGBE_LINKS 0x042A4
/* 10GbE Signal detection for KR or SFI */
#define IXGBE_LINKS_SIG 12
/* 10GbE Align status */
#define IXGBE_LINKS_ALIGN 17
#define IXGBE_LINKS_SPEED_SHIFT 28
#define IXGBE_LINKS_SPEED_MASK (0x3 << IXGBE_LINKS_SPEED_SHIFT)
#define IXGBE_LINKS_UP 30

/* Auto Negotiation Control 2 Register */
#define IXGBE_AUTOC2 0x042A8

/* DMA Tx Control */
#define IXGBE_DMATXCTL 0x04A80
#define IXGBE_DMATXCTL_TE (1 << 0)

/* Receive Checksum Control */
#define IXGBE_RXCSUM 0x05000
#define IXGBE_RXCSUM_IPPCSE (1 << 12)
#define IXGBE_RXCSUM_PCSD (1 << 13)

/* Receive Filter Control Register */
#define IXGBE_RFCTL 0x05008
#define IXGBE_RFCTL_RSC_DIS (1 << 5)
#define IXGBE_RFCTL_NFSW_DIS (1 << 6)
#define IXGBE_RFCTL_NFSR_DIS (1 << 7)
#define IXGBE_RFCTL_IPV6_DIS (1 << 10)
#define IXGBE_RFCTL_IPFRSP_DIS (1 << 14)

/* Multicast Table Array */
#define IXGBE_MTA 0x05200

/* Filter Control Register */
#define IXGBE_FCTRL 0x05080
#define IXGBE_FCTRL_SBP (1 << 1) // Store Bad Packets
#define IXGBE_FCTRL_MPE (1 << 8) // Multicast Promiscuous Enable
#define IXGBE_FCTRL_UPE (1 << 9) // Unicast Promiscuous Enable
#define IXGBE_FCTRL_BAM (1 << 10) // Broadcast Accept Mode

/* VLAN Control Register */
#define IXGBE_VLNCTRL 0x05088
#define IXGBE_VLNCTRL_VFE (1 << 30) // VLAN Filter Enable.

/* Multicast Control Register */
#define IXGBE_MCSTCTRL 0x05090
#define IXGBE_MCSTCTRL_MFE (1 << 2)

/* RSS Queues Per Traffic Class Register */
#define IXGBE_RQTC 0x0EC70

/* EType Queue Filter */
#define IXGBE_ETQF 0x05128

/* Rx Filter ECC Err Insertion 0 */
#define IXGBE_RXFECCERR0 0x051B8
#define IXGBE_RXFECCERR0_ECCFLT_EN (1 << 9)

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
#define IXGBE_RAH_AV (1U << 31)

/* MAC Pool Select Array */
#define IXGBE_MPSAR 0x0A600

/* Receive Descriptor Base Address Low */
#define IXGBE_RDBAL 0x01000
/* Receive Descriptor Base Address High */
#define IXGBE_RDBAH 0x01004
/* Receive Descriptor Length */
#define IXGBE_RDLEN 0x01008
/* Transmit Descriptor Base Address Low*/
#define IXGBE_TDBAL 0x06000
/* Transmit Descriptor Base Address High */
#define IXGBE_TDBAH 0x06004
/* Transmit Descriptor Length*/
#define IXGBE_TDLEN 0x06008

/* Receive Descriptor Head */
#define IXGBE_RDH 0x01010

/* Receive Descriptor Tail */
#define IXGBE_RDT 0x01018
/* Transmit Descriptor Head*/
#define IXGBE_TDH 0x06010
/* Transmit Descriptor Tail */
#define IXGBE_TDT 0x06018

/* Receive Descriptor Control */
#define IXGBE_RXDCTL 0x01028
#define IXGBE_RXDCTL_RX_EN  (1 << 25)
#define IXGBE_RXDCTL_VME    (1 << 30)
/* Transmit Descriptor Control */
#define IXGBE_TXDCTL 0x06028
#define IXGBE_TXDCTL_PTHRESH 0x0000003F /* Bits 6:0 */
#define IXGBE_TXDCTL_HTHRESH 0x00007F00 /* Bits 14:8 */
#define IXGBE_TXDCTL_WTHRESH 0x007F0000 /* Bits 22:16 */
#define IXGBE_TXDCTL_TX_EN  (1 << 25)

/* Split Receive Control Registers */
#define IXGBE_SRRCTL 0x01014
#define IXGBE_SRRCTL_BSIZEPACKET 0x0000000F /* Bits 4:0 */
#define IXGBE_SRRCTL_DESCTYPE 0x0E000000 /* Bits 27:25*/

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

/* Packet Split Receive Type Register */
#define IXGBE_PSRTYPE 0x0EA00
#define IXGBE_PSRTYPE_TCPHDR      (1 << 4)
#define IXGBE_PSRTYPE_UDPHDR      (1 << 5)
#define IXGBE_PSRTYPE_IPV4HDR     (1 << 8)
#define IXGBE_PSRTYPE_IPV6HDR     (1 << 9)
#define IXGBE_PSRTYPE_L2HDR       (1 << 12)
#define IXGBE_PSRTYPE_SPLIT_MASK  0x00001FFF /* Bits 12:0 */
#define IXGBE_PSRTYPE_RQPL_SHIFT  29

/* SYN Packet Queue Filter */
#define IXGBE_SYNQF 0x0EC30

/* EType Queue Select */
#define IXGBE_ETQS 0x0EC00

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
#define IXGBE_SECRXCTRL_RX_DIS (1 << 1)

/* Security Rx Status */
#define IXGBE_SECRXSTAT 0x08D04

/* DCA Rx Control Register */
#define IXGBE_DCA_RXCTRL 0x0100C
/* Rezerved, but needs to be 0 */
#define IXGBE_DCA_RXCTRL_SET_TO_0 (1 << 12)

/* Semaphore Register */
#define IXGBE_SWSM 0x10140
#define IXGBE_SWSM_SMBI (1 << 0)
#define IXGBE_SWSM_SWESMBI (1 << 1)
#define IXGBE_SWSM_MASK (IXGBE_SWSM_SMBI | IXGBE_SWSM_SWESMBI)

/* Software _ Firmware Synchronization */
#define IXGBE_SW_FW_SYNC 0x10160

/* Trace registers start */
/* Good Packets Received Count Register */
#define IXGBE_GPRC 0x04074
/* Good Packets Transmitted Count Register */
#define IXGBE_GPTC 0x04080
/* Total Packets Received Register */
#define IXGBE_TPR 0x040D0
/* Total Packets Transmitted Register */
#define IXGBE_TPT 0x040D4
/* Rx Missed Packet Count*/
#define IXGBE_RXMPC 0x03FA0

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

/* PCIe Control Extended Register */
#define IXGBE_GCR_EXT 0x11050
#define IXGBE_GCR_EXT_BUFFERS_CLEAR_FUNC (1 << 30)

/* DCB Transmit Descriptor Plane Control and Status 
 * For summary, "Data Center Bridging (DCB) is a collection of standards-based extensions to classical Ethernet.
 * It provides a lossless data center transport layer." 
 * - https://edc.intel.com/content/www/us/en/design/products/ethernet/adapters-and-devices-user-guide/data-center-bridging-dcb/
 */
#define IXGBE_RTTDCS 0x04900
#define IXGBE_RTTDCS_ARBDIS (1 << 6)

/* DMA Tx TCP Max Allow Size Requests */
#define IXGBE_DTXMXSZRQ 0x08100
#define IXGBE_DTXMXSZRQ_MAX_REQ 0x00000FFE /* Max_bytes_num_req. Bits 11:0 */
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
void master_disable_workaround(const struct hw* hw);
int rx_ring_probe(const struct hw* hw);
int tx_ring_probe(const struct hw* hw);
void clock_switching_workaround(const struct hw* hw);

#endif