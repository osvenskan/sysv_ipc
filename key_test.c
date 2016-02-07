
#include <sys/ipc.h>

int main(void) { 
#ifdef _DARWIN_C_SOURCE
    int a = 1;
#endif
#ifdef _XOPEN_SOURCE
    int b = 1;
#endif
#ifdef _POSIX_C_SOURCE
    int c = 1;
#endif
#ifdef __LP64__
    int d = 1;
#endif
#ifdef __arm__
    int e = 1;
#endif
#ifdef KERNEL
    int f = 1;
#endif
#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
    int g = __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__;
#endif

    int z = __DARWIN_UNIX03;

    
    struct ipc_perm foo; 
    foo.key = 42; 
    return 0; 
}
