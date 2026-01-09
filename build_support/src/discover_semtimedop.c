#include <sys/sem.h>
#include <stdlib.h>

int main(void) { 
    semtimedop(0, NULL, 0, NULL); 

    return 0;
}
