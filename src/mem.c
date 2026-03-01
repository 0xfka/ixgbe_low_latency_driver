#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "base.h"
#include "hw.h"
/*
 * Alloc 2 MB hugepage. Requires at least 1 available 2 MB hugepage on host
 * kernel. Dividing the same hugepage is aiming to reduce TLB misses.
 */
int alloc_hugepage(struct hw* hw) {
  int save_errno;
  void* dma = mmap(NULL, 2 * 1024 * 1024, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (unlikely(dma == MAP_FAILED)) {
    save_errno = errno;
    write(2, "mmap failed\n", 12);
    return -save_errno;
  };
  memset(dma, 0, 2 * 1024 * 1024);
  hw->rx_base = dma;
  hw->tx_base = (u8*)dma + (1 * 128 * 1024);
  return 0;
}
/*
 * Since Linux returns virtual mem addr in alloc_hugepage function,
 * it needs to be converted to physical addr to use on NIC.
 */
int virt2phy(struct hw* hw) {
  int save_errno;
  const u64 v_addr = (u64)hw->rx_base;
  const long pagesize = sysconf(_SC_PAGESIZE);
  if (unlikely(pagesize <= 0)) return -errno;
  const u64 index = v_addr / (u64)pagesize;

  const int fd = open("/proc/self/pagemap", O_RDONLY);
  u64 result;

  if (unlikely(fd < 0)) {
    return -errno;
  }
  if (unlikely(lseek(fd, index * sizeof(u64), SEEK_SET) < 0)) {
    save_errno = errno;
    close(fd);
    return -save_errno;
  }
  if (unlikely(read(fd, &result, sizeof(result)) != sizeof(result))) {
    save_errno = errno;
    close(fd);
    return -(save_errno ? save_errno : EIO);
  }
  close(fd);
  // Checks is the addr is in ram or not. ( Swap Etc. )
  if (unlikely(!(result & (1ULL << 63)))) {
    /* Couldn't return errno since swapped memory is not an error, but unusable
     * for our workflow. */
    return -EFAULT;
  }
  u64 paddr = (result & ((1ULL << 55) - 1)) * (u64)pagesize;
  paddr += v_addr % (u64)pagesize;
  hw->rx_base_phy = paddr;
  return 0;
}
/*
 * Implement memory map to access NIC registers from userspace.
 */
int mmap_bar0(struct hw* hw) {
  char path[128];
  const int len = snprintf(path, sizeof(path),
                           "/sys/bus/pci/devices/%s/resource0", hw->pci_addr);
  if (unlikely(len < 0 || (size_t)len >= sizeof(path))) {
    return -EINVAL;
  }
  const int fd = open(path, O_RDWR | O_SYNC);
  /* O_SYNC flag ensures that we can edit NIC registers instantly.
  Without it, CPU may cause latency between ;
  editing register on userspace - writing to NIC. */
  if (unlikely(fd < 0)) {
    return -errno;
  }
  void* mmio =
      mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  int save_errno = errno;
  close(fd);
  if (unlikely(mmio == MAP_FAILED)) {
    return -save_errno;
  }
  hw->hw_addr = mmio;
  return 0;
}