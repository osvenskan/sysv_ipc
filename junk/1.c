
#define _XOPEN_SOURCE 600
#include <sys/ipc.h>
int main(void) { struct ipc_perm foo; foo._seq = 42; }

