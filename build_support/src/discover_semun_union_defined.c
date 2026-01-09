#include <sys/sem.h>

int main(void) {
    union semun foo;
    
    foo.val = 42;
    
    return 0;
}
