#define _GNU_SOURCE
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <signal.h>
#include <assert.h>
static unsigned char *destination;
static size_t region_size,page_size;
static volatile sig_atomic_t faults;
static ssize_t guarded_read(int fd,void *buf,size_t size);
static ssize_t guarded_pread(int fd,void *buf,size_t size,off_t off);
#define read guarded_read
#define pread guarded_pread
#ifdef BB_OLD_FILE_READ
#include "/tmp/uc2-old-runtime_file.c"
#else
#include "../src/runtime_file.c"
#endif
#undef read
#undef pread
static int32_t guest_errno;
int32_t *runtime_errno(void){return &guest_errno;}
int32_t runtime_guest_errno(int e){return e;}
uintptr_t runtime_lookup(const RuntimeExport *table,size_t n,const char *name){
    for(size_t i=0;i<n;++i)if(!strcmp(table[i].name,name))return(uintptr_t)table[i].function;
    return 0;
}
static void protect_destination(void){
    assert(!mprotect(destination+page_size,region_size-page_size,PROT_READ));
}
static ssize_t guarded_read(int fd,void *buf,size_t size){
    protect_destination();return syscall(SYS_read,fd,buf,size);
}
static ssize_t guarded_pread(int fd,void *buf,size_t size,off_t off){
    protect_destination();return syscall(SYS_pread64,fd,buf,size,off);
}
static void fault(int sig,siginfo_t *info,void *context){
    (void)sig;(void)context;
    if((uintptr_t)info->si_addr<(uintptr_t)destination ||
       (uintptr_t)info->si_addr>=(uintptr_t)destination+region_size)_exit(90);
    if(mprotect(destination,region_size,PROT_READ|PROT_WRITE))_exit(91);
    ++faults;
}
int main(void){
    page_size=(size_t)sysconf(_SC_PAGESIZE);region_size=page_size*80;
    destination=mmap(NULL,region_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(destination!=MAP_FAILED);
    unsigned char *expected=malloc(region_size);assert(expected);
    for(size_t i=0;i<region_size;++i)expected[i]=(unsigned char)(i*17+i/251);
    char name[]="/tmp/uc2-protected-read-XXXXXX";int fd=mkstemp(name);assert(fd>=3);unlink(name);
    assert(write(fd,expected,region_size)==(ssize_t)region_size);
    struct sigaction sa={0};sa.sa_sigaction=fault;sa.sa_flags=SA_SIGINFO;
    assert(!sigaction(SIGSEGV,&sa,NULL));
    assert(lseek(fd,0,SEEK_SET)==0);
    ssize_t got=read_full(fd,destination,region_size,-1);
#ifdef BB_OLD_FILE_READ
    assert(got!=(ssize_t)region_size);
    puts("REPRODUCED: original read returns truncated data when GPU protection is reapplied");
#else
    assert(got==(ssize_t)region_size && !memcmp(destination,expected,region_size));
    assert(faults>0);
    assert(!mprotect(destination,region_size,PROT_READ|PROT_WRITE));memset(destination,0,region_size);
    assert(read_full(fd,destination,region_size,0)==(ssize_t)region_size);
    assert(!memcmp(destination,expected,region_size));
    assert(lseek(fd,0,SEEK_CUR)==(off_t)region_size); // pread leaves offset intact
    assert(read_full(fd,destination,0,0)==0);
    assert(read_full(fd,destination,page_size,(int64_t)region_size-page_size/2)==(ssize_t)(page_size/2));
    assert(read_full(fd,destination,page_size,(int64_t)region_size)==0);
    errno=0;assert(read_full(-1,destination,page_size,0)==-1 && errno==EBADF);
    puts("PASS: protected read/pread, multiple chunks, EOF, zero size, errors and file offsets");
#endif
    close(fd);munmap(destination,region_size);free(expected);return 0;
}
