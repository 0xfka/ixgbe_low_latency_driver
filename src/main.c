#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>
#include <xmmintrin.h>

#include "../selftests/selftests.h"
#include "base.h"
#include "exit_path.h"
#include "hdr/hdr_histogram.h"
#include "hw.h"
#include "iex.h"
#include "ixgbe.h"
#include "management.h"
#include "pci.h"
struct hw ixgbe_adapter __attribute__((aligned(64))) = {0};
static struct ixgbe_stats stats = {0};
static struct spsc_ring_hugepage_layout* spsc = {0};

volatile sig_atomic_t run = true;
void handle_sigint(int sig) { run = false; }
int main(const int argc, char** argv) {
  signal(SIGINT, handle_sigint);
  int err;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  err = CPU_ISSET(0, &cpuset);
  if (unlikely(sched_setaffinity(CORE_DATAPATH, sizeof(cpu_set_t), &cpuset) !=
               CORE_DATAPATH)) {
    return errno;
  }
  err = ixgbe_test_ds();
  if (unlikely(err != 0)) {
    return -err;
  }
  err = iex_test_ds();
  if (unlikely(err != 0)) {
    return -err;
  }
  if (unlikely(argc < 2)) {
    return EINVAL;
  }
  if (unlikely(argv[1] == NULL)) {
    return EINVAL;
  }
  ixgbe_adapter.pci_addr = argv[1];
  // Driver should be changed for another PCI direct access modes.
  err = unbind(ixgbe_adapter.pci_addr, "uio_pci_generic");
  if (unlikely(err != 0)) {
    return -err;
  }
  err = alloc_hugepage(&ixgbe_adapter);
  if (unlikely(err != 0)) {
    return -err;
  }
  err = virt2phy((u64)ixgbe_adapter.rx_base, &ixgbe_adapter.rx_base_phy);
  if (unlikely(err != 0)) {
    return -err;
  }
  err = virt2phy((u64)ixgbe_adapter.tx_base, &ixgbe_adapter.tx_base_phy);
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
  err = (mlockall(MCL_CURRENT | MCL_FUTURE));
  if (unlikely(err != 0)) {
    return -err;
  }
  union ixgbe_adv_rx_desc* rx_ring =
      (union ixgbe_adv_rx_desc*)ixgbe_adapter.rx_base;
  union ixgbe_adv_tx_desc* tx_ring =
      (union ixgbe_adv_tx_desc*)ixgbe_adapter.tx_base;
  ixgbe_write_reg(&ixgbe_adapter, IXGBE_RDT, BUFFER_NUMBER - 1);
  ixgbe_read_reg(&ixgbe_adapter, IXGBE_GPRC);
  ixgbe_read_reg(&ixgbe_adapter, IXGBE_RXMPC);
  u32 read_val = ixgbe_read_reg(&ixgbe_adapter, IXGBE_AUTOC);
  IXGBE_SET_BITS(read_val, IXGBE_AUTOC_RESTART);
  ixgbe_write_reg(&ixgbe_adapter, IXGBE_AUTOC, read_val);
  /* For management thread*/
  void* hugepage = mmap(NULL, 2 * 1024 * 1024, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (unlikely(hugepage == MAP_FAILED)) {
    return errno;
  };
  /* mmap may act lazy when allocating it. */
  memset(hugepage, 0, 2 * 1024 * 1024);
  struct spsc_ring_hugepage_layout* spsc =
      (struct spsc_ring_hugepage_layout*)hugepage;
  pthread_t thread_management;
  cpu_set_t cpuset_management;
  CPU_ZERO(&cpuset_management);
  CPU_SET(CORE_MANAGEMENT, &cpuset_management);
  pthread_create(&thread_management, NULL, management_entrypoint, spsc);
  pthread_setaffinity_np(thread_management, sizeof(cpu_set_t),
                         &cpuset_management);
  printf("Main sees ring at: %p\n", (void*)spsc);
  fflush(stdout);
  /* This register is used for updating ring buffer location on every x bytes.
   * 128 is a placeholder, a number will be decided after benchmarks. */
  stats.batch_manage_tail = 128;
  stats.batch_manage_tail_counter = 0;
  stats.batch_tx_counter = 0;
  /* Basic benchmarks show that batching Tx is increasing latency too much.
   * Benchmarks will be added before merging this branch to main.
   */
  stats.batch_tx_transmit = 0;
  u32 i = ixgbe_read_reg(&ixgbe_adapter, IXGBE_RDH);
  struct hdr_histogram* latency_hist;
  hdr_init(1, 10000000, 3, &latency_hist);
  u32 tx_write = 0; /* Next to write */
  u32 tx_clean = 0; /* Oldest to clean */
  while (run) {
    barrier();
    if (likely(rx_ring[i].wb.status_error & IXGBE_RXD_STAT_DD)) {
      rmb();
      u64 start_cycles, end_cycles;
      start_cycles = __rdtsc();
      u8* pkt = (u8*)ixgbe_adapter.rx_base + (256 * 1024) + (i * 2048);
      _mm_prefetch(pkt, _MM_HINT_T0);
      stats.batch_manage_tail_counter++;
      stats.total_packets++;
      if (unlikely(((tx_clean - tx_write - 1) & (BUFFER_NUMBER - 1)) <=
                   stats.batch_manage_tail)) {
        if (tx_ring[tx_clean].data_wb.sta & IXGBE_RXD_STAT_DD) {
          tx_ring[tx_clean].data_wb.sta &= ~IXGBE_RXD_STAT_DD;
          tx_clean = (tx_clean + 1) & (BUFFER_NUMBER - 1);
        }
      }
      stats.total_bytes_rx = stats.total_bytes_rx + rx_ring[i].wb.length;
      if (unlikely(stats.batch_manage_tail_counter >=
                   stats.batch_manage_tail)) {
        ixgbe_write_reg(&ixgbe_adapter, IXGBE_RDT, i);
        stats.batch_manage_tail_counter = 0;
      }
      /* Temporarily switched to true for benchmarking CPU cycles. */
      bool processed = true;
      /* Jumping directly from ethhdr (14), iphdr (20) and udphdr (8) */
      struct IEX_TP_header* tp = (struct IEX_TP_header*)(pkt + 20 + 14 + 8);
      if (unlikely(tp->version != 1)) {
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        wmb();
        rx_ring[i].read.pkt_addr =
            (u64)ixgbe_adapter.rx_base_phy + (256 * 1024) + (i * 2048);
        rx_ring[i].read.hdr_addr = 0;
        i = (i + 1) & (BUFFER_NUMBER - 1);
        stats.irrelevant_packets++;
        continue;
      }
      static bool seqnum_first_pkt = true;
      static bool sessionid_first_pkt = true;
      static u32 expected_session_id;
      static u64 expected_seqnum;
      if (unlikely(tp->mesg_count <= 0)) {
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        wmb();
        rx_ring[i].read.pkt_addr =
            (u64)ixgbe_adapter.rx_base_phy + (256 * 1024) + (i * 2048);
        rx_ring[i].read.hdr_addr = 0;
        i = (i + 1) & (BUFFER_NUMBER - 1);
        stats.irrelevant_packets++;
        continue;
      }
      if (unlikely(sessionid_first_pkt)) {
        expected_session_id = tp->session_id;
        sessionid_first_pkt = false;
      }
      if (unlikely(seqnum_first_pkt)) {
        expected_seqnum = tp->first_mesg_seq_num;
        seqnum_first_pkt = false;
      }
      if (unlikely(tp->first_mesg_seq_num != expected_seqnum)) {
        run = false;
      }
      if (unlikely(tp->session_id != expected_session_id)) {
        run = false;
      }
      expected_seqnum += tp->mesg_count;
      u8* msg = (u8*)tp + sizeof(struct IEX_TP_header);
      for (u32 m = 0; m < tp->mesg_count; m++) {
        u16 msg_len = *(u16*)msg;
        u8 msg_type = *(u8*)(msg + 2);
        switch (msg_type) {
          case IEX_SSPTSM: {
            struct IEX_Short_Sale_Price_Test* iex =
                (struct IEX_Short_Sale_Price_Test*)(msg + 2);
            printf("%.8s\n", iex->symbol);
            break;
          }
          case IEX_SYSTEM_EVENT: {
            struct IEX_System_Event* iex = (struct IEX_System_Event*)(msg + 2);
            if (unlikely(iex->IEX_System_Event_t == IEX_END_OF_MESSAGES ||
                         iex->IEX_System_Event_t == IEX_END_OF_SYSTEM_HOURS ||
                         iex->IEX_System_Event_t ==
                             IEX_END_OF_REGULAR_MARKET_HOURS)) {
              run = false;
            }
            if (unlikely(iex->IEX_System_Event_t ==
                         IEX_START_OF_REGULAR_MARKET_HOURS)) {
              run = false;
            }
            break;
          }
          default: {
            stats.irrelevant_messages++;
            break;
          }
        }
        msg += msg_len;
      }
      if (unlikely((((tx_write + 1) & (BUFFER_NUMBER - 1)) == tx_clean))) {
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        wmb();
        rx_ring[i].read.pkt_addr =
            (u64)ixgbe_adapter.rx_base_phy + (256 * 1024) + (i * 2048);
        rx_ring[i].read.hdr_addr = 0;
        i = (i + 1) & (BUFFER_NUMBER - 1);
        stats.ring_full_drop++;
        continue;
      }
      if (unlikely(!processed)) {
        rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
        wmb();
        rx_ring[i].read.pkt_addr =
            (u64)ixgbe_adapter.rx_base_phy + (256 * 1024) + (i * 2048);
        rx_ring[i].read.hdr_addr = 0;
        i = (i + 1) & (BUFFER_NUMBER - 1);
        stats.irrelevant_packets++;
        continue;
      }
      rx_ring[i].wb.status_error &= ~IXGBE_RXD_STAT_DD;
      wmb();
      rx_ring[i].read.pkt_addr =
          (u64)ixgbe_adapter.rx_base_phy + (256 * 1024) + (tx_write * 2048);
      rx_ring[i].read.hdr_addr = 0;
      i = (i + 1) & (BUFFER_NUMBER - 1);
      tx_write = (tx_write + 1) & (BUFFER_NUMBER - 1);
      if (unlikely(stats.batch_tx_counter >= stats.batch_tx_transmit)) {
        ixgbe_write_reg(&ixgbe_adapter, IXGBE_TDT, tx_write);
        stats.batch_tx_counter = 0;
      } else {
        stats.batch_tx_counter++;
      }
      end_cycles = __rdtsc();
      hdr_record_value(latency_hist, end_cycles - start_cycles);
    }
    _mm_pause();
  }
  hdr_percentiles_print(latency_hist, stdout, 5, 1.0, CLASSIC);
  hdr_close(latency_hist);
  exit_entrypoint(&stats, &ixgbe_adapter);
}