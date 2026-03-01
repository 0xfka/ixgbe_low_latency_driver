#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "base.h"
/*
 * Taking the device from kernel's control and binds to target_drv.
 */
int unbind(const char* pci, const char* target_drv) {
  char path[128];
  int save_errno;
  ssize_t w;
  if (unlikely(pci == NULL)) return -EINVAL;
  const size_t pci_len = strnlen(pci, 64);
  if (unlikely(pci_len >= 64)) return -ENAMETOOLONG;
  if (unlikely(target_drv == NULL)) return -EINVAL;
  if (unlikely(strnlen(target_drv, 64) >= 64)) return -ENAMETOOLONG;
  int len = snprintf(path, sizeof(path),
                     "/sys/bus/pci/devices/%s/driver/unbind", pci);
  if (unlikely(len < 0)) return -EINVAL;
  if (unlikely((size_t)len >= sizeof(path))) return -ENAMETOOLONG;
  int fd = open(path, O_WRONLY);
  if (fd >= 0) {
    w = write(fd, pci, pci_len);
    if (unlikely(w != (ssize_t)pci_len)) {
      save_errno = errno;
      close(fd);
      return -(save_errno ? save_errno : EIO);
    }
    close(fd);
  } else
    return -EINVAL;
  len = snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/driver_override",
                 pci);
  if (unlikely(len < 0)) return -EINVAL;
  if (unlikely((size_t)len >= sizeof(path))) return -ENAMETOOLONG;
  fd = open(path, O_WRONLY);
  if (unlikely(fd < 0)) {
    return -(errno ? errno : EIO);
  }
  char buf[64];
  len = snprintf(buf, sizeof(buf), "%s\n", target_drv);
  if (unlikely(len < 0)) {
    close(fd);
    return -EINVAL;
  }
  if (unlikely((size_t)len >= sizeof(buf))) {
    close(fd);
    return -ENAMETOOLONG;
  }

  w = write(fd, buf, len);
  if (unlikely(w != (ssize_t)len)) {
    save_errno = errno;
    close(fd);
    return -(save_errno ? save_errno : EIO);
  }
  close(fd);

  fd = open("/sys/bus/pci/drivers_probe", O_WRONLY);
  if (unlikely(fd < 0)) return (errno ? errno : EIO);
  w = write(fd, pci, pci_len);
  if (unlikely(w != (ssize_t)pci_len)) {
    save_errno = errno;
    close(fd);
    return -(save_errno ? save_errno : EIO);
  }
  close(fd);
  /* Enable DMA */
  len = snprintf(path, sizeof(path),"/sys/bus/pci/devices/%s/config",pci);
  if (unlikely(len < 0)) return -EINVAL;
  if (unlikely((size_t)len >= sizeof(path))) return -ENAMETOOLONG;
  fd = open(path, O_RDWR);
  if (unlikely(fd < 0)) {
    return -(errno ? errno : EIO);
  }
  if (lseek(fd, 4, SEEK_SET) != 4){
    save_errno = errno;
    close(fd);
    return -(save_errno ? save_errno : EIO);
  }
  u16 read_val = 0;
  if (read(fd, &read_val, 2) != 2){
    save_errno = errno;
    close(fd);
    return -(save_errno ? save_errno : EIO);
  }
  read_val |= 1 << 2;
  if (lseek(fd, 4, SEEK_SET) != 4){
    save_errno = errno;
    close(fd);
    return -(save_errno ? save_errno : EIO);
  }
  if (write(fd, &read_val, 2) != 2){
    save_errno = errno;
    close(fd);
    return -(save_errno ? save_errno : EIO);
  }
  close(fd);
  return 0;
}