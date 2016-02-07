//#include <sys/sem.h>
//#include <stdio.h>

// #ifdef _SEM_SEMUN_UNDEFINED
// union semun {
//     int val;                    /* used for SETVAL only */
//     struct semid_ds *buf;       /* for IPC_STAT and IPC_SET */
//     unsigned short *array;      /* used for GETALL and SETALL */
// };
// #endif
// 
// 
// union semun _sysv_ipc_junk;
// #define SEMUN_VAL_MAX ((((unsigned long)1) << ((sizeof(_sysv_ipc_junk.val) * 8) - 1)) - 1)
// 
// #ifdef SMVMX
// #define SEMAPHORE_VALUE_MAX ( SEMVMX > (SEMUN_VAL_MAX > LONG_MAX) ? LONG_MAX : SEMUN_VAL_MAX) ? )
// #else
// #define SEMAPHORE_VALUE_MAX ( (SEMUN_VAL_MAX > LONG_MAX) ? LONG_MAX : SEMUN_VAL_MAX )
// #endif
// 

#include <stdio.h>

typedef long foo_t;   // This represents my unknown type

int main (int argc, char const *argv[])
{
    foo_t test_value;
    foo_t max = 127;
    int found = 0;
    
    while (!found) {
        test_value = (max * 2) + 1;
        if ( (test_value <= max) || (test_value != ((max * 2) + 1)))
            // overflow!
            found = 1;
        else
            max = test_value;
    }
    
    printf("max: %llu\n", (unsigned long long)max);

    return 0;
}
