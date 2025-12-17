//#define _XOPEN_SOURCE  500
#include <sys/sem.h>

int main(void) {
    union semun foo;
    
    foo.val = 42;
    
    return 0;
}
