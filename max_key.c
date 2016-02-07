#include <math.h>
#include <stdio.h>
#include <sys/ipc.h>		/* for system's IPC_xxx definitions */


union semun {
    int val;                    /* used for SETVAL only */
    struct semid_ds *buf;       /* for IPC_STAT and IPC_SET */
    unsigned short *array;      /* used for GETALL and SETALL */
};

union semun _ignoreme;
#define SEMUN_VAL_MAX ((((unsigned long)1) << ((sizeof(_ignoreme.val) * 8) - 1)) - 1)

int main (int argc, char const *argv[])
{
    int i;
    key_t j;
    int size = 4;
    
    
    
    printf("sizeof(union semun.val) = %d\n", sizeof(_ignoreme.val) );
    printf("SEMUN_VAL_MAX = %d\n", SEMUN_VAL_MAX);
    printf("sizeof(key_t) = %d\n", sizeof(key_t));

    
    for (i = 0; i <= 32; i++) {
        
        
        
        printf("%d: %ld\n", i, (unsigned long)pow(2, ((size * 8) - 1)));
        
        j = 1;
        // # of bits minus one for sign
        j = j << ((size * 8) - 1);
        
        printf("%d: %ld\n", i, j);

        j = 1;
        printf("%d: %ld\n\n", i, (( ((unsigned long)1) << ((size * 8) - 1)) - 1));

    }
    
    return 0;
}


