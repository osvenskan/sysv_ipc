#include <stdlib.h>
#include <stdio.h> 
#include <limits.h>

int main() { 
    int i;
    int key;
    

    for (i = 0; i < 10000000; i++) {
        // ref: http://www.c-faq.com/lib/randrange.html
        key = ((int)((double)rand() / ((double)RAND_MAX + 1) * INT_MAX)) + 1;
        printf("%d\n", key);
    }
    
 
    
    return 1;
}