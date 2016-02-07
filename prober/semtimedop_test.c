#define _XOPEN_SOURCE 600

#include <sys/sem.h>
#include <stdlib.h>

int main(void) { semtimedop(0, NULL, 0, NULL); }
