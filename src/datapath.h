#ifndef DATAPATH_H
#define DATAPATH_H
#include "debug.h"
#include "ixgbe.h"
#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
static inline bool ping_reply(struct ethhdr* eth,struct iphdr* ip, struct icmphdr* icmp,struct ixgbe_stats* stats,int i,union ixgbe_adv_rx_desc* rx_ring, union ixgbe_adv_tx_desc* tx_ring){
     if(unlikely(ip->version != 4)){
        rx_ring[i].wb.status_error = 0;
        stats->irrelevant_packets++;
        return false;
      }
      if(unlikely((ip->protocol != 1))){
        rx_ring[i].wb.status_error = 0;
        stats->irrelevant_packets++;
        return false;
      }
      /* After the checks, there's no branch before transmitting. */
      stats->total_bytes_tx = stats->total_bytes_tx + ip->tot_len;
      /* Swap MAC's */
      for (int j = 0; j < 6; j++) {
      u8 tmp = eth->h_source[j];
      eth->h_source[j] = eth->h_dest[j];
      eth->h_dest[j] = tmp;
      }
      /* Swap IP's */
      u32 tmp_ip = ip->daddr;
      ip->daddr = ip->saddr;
      ip->saddr = tmp_ip;
      /* Change type to reply on ICMP */
      if(likely(icmp->type == 8)){
        icmp->type = 0;
      /* Calculate new checksum with RFC 1624 */
      u32 chk_new = ntohs(icmp->checksum) + 0x0800;
      if (unlikely(chk_new > 0xFFFF))
        chk_new = (chk_new & 0xFFFF) +1;
      icmp->checksum = htons(chk_new);
      }
      u64 transmit = ixgbe_adapter.rx_base_phy + (256 * 1024) + ( i * 2048);
      union ixgbe_adv_tx_desc *tx_desc = &tx_ring[i];
      tx_desc->data_read.address = transmit;
      tx_desc->data_read.dtalen = rx_ring[i].wb.length;
      tx_desc->data_read.paylen = rx_ring[i].wb.length;
      tx_desc->data_read.dtyp = 3;
      tx_desc->data_read.rs = 1;
      tx_desc->data_read.eop = 1;
      tx_desc->data_read.ifcs = 1;
      tx_desc->data_read.dext = 1;
      tx_desc->data_read.dd = 0;
      return true;
}
#endif