/* kgslpwr <0|1> [seconds]: IOCTL_KGSL_SETPROPERTY(KGSL_PROP_PWRCTRL, value) on /dev/kgsl-3d0 and,
 * with seconds, keeps the fd open that long (prints ifpc_count before and after), then sets 1. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct kgsl_device_getproperty {
   unsigned int type;
   void *value;
   size_t sizebytes;
};
#define KGSL_IOC_TYPE 0x09
#define IOCTL_KGSL_SETPROPERTY _IOW(KGSL_IOC_TYPE, 0x32, struct kgsl_device_getproperty)
#define KGSL_PROP_PWRCTRL 0xE

static long ifpc(void) {
   char buf[64] = {0};
   int fd = open("/sys/class/kgsl/kgsl-3d0/ifpc_count", O_RDONLY);
   if (fd < 0) return -1;
   read(fd, buf, sizeof(buf) - 1);
   close(fd);
   return strtol(buf, NULL, 0);
}

static int set(int fd, unsigned int v) {
   struct kgsl_device_getproperty p = {KGSL_PROP_PWRCTRL, &v, sizeof(v)};
   int r = ioctl(fd, IOCTL_KGSL_SETPROPERTY, &p);
   printf("SETPROPERTY PWRCTRL %u -> %d (%s)\n", v, r, r ? strerror(errno) : "ok");
   return r;
}


int main(int argc, char **argv) {
   unsigned int v = argc > 1 ? atoi(argv[1]) : 0;
   int secs = argc > 2 ? atoi(argv[2]) : 0;
   int fd = open("/dev/kgsl-3d0", O_RDWR);
   if (fd < 0) { perror("open /dev/kgsl-3d0"); return 1; }
   long a = ifpc();
   set(fd, v);
   if (secs) {
      sleep(secs);
      long b = ifpc();
      printf("ifpc_count %ld -> %ld over %d s\n", a, b, secs);
      set(fd, 1);
   }
   close(fd);
   return 0;
}
