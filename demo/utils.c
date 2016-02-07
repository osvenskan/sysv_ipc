#include <time.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <errno.h> 
#include <semaphore.h>
#include <string.h>


#include <sys/ipc.h>		/* for system's IPC_xxx definitions */
#include <sys/shm.h>		/* for shmget, shmat, shmdt, shmctl */
#include <sys/sem.h>		/* for semget, semctl, semop */

#include "utils.h"
#include "md5.h"


void md5ify(char *inString, char *outString) {
	md5_state_t state;
	md5_byte_t digest[16];
    int i;
    
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)inString, strlen(inString));
	md5_finish(&state, digest);

    for (i = 0; i < 16; i++)
        sprintf(&outString[i * 2], "%02x", digest[i]);
}

void say(const char *pName, char *pMessage) {
    time_t the_time;
    struct tm *the_localtime;
    char timestamp[256];
    
    the_time = time(NULL);
    
    the_localtime = localtime(&the_time);
    
    strftime(timestamp, 255, "%H:%M:%S", the_localtime);
    
    printf("%s @ %s: %s\n", pName, timestamp, pMessage);
    
}


int release_semaphore(const char *pName, int sem_id, int live_dangerously) {
    int rc = 0;
    struct sembuf op[1];
    char s[1024];
    
    say(pName, "Releasing the semaphore.");
    
    if (!live_dangerously) {
        op[0].sem_num = 0;
        op[0].sem_op = 1;
        op[0].sem_flg = 0;

        if (-1 == semop(sem_id, op, (size_t)1)) {
            sprintf(s, "Releasing the semaphore failed; errno is %d\n", errno);
            say(pName, s);
        }
    }
    
    return rc;
}


int acquire_semaphore(const char *pName, int sem_id, int live_dangerously) {
    int rc = 0;
    struct sembuf op[1];
    char s[1024];

    say(pName, "Waiting to acquire the semaphore.");

    if (!live_dangerously) {
        op[0].sem_num = 0;
        op[0].sem_op = -1;
        op[0].sem_flg = 0;
        if (-1 == semop(sem_id, op, (size_t)1)) {
            sprintf(s, "Acquiring the semaphore failed; errno is %d\n", errno);
            say(pName, s);
        }
    }

    return rc;
}


void read_params(struct param_struct *params) {
    char line[1024];
    char name[1024];
    int value = 0;
    
    FILE *fp;
    
    fp = fopen("params.txt", "r");
    
    while (fgets(line, 1024, fp)) {
        if (strlen(line) && ('#' == line[0]))
            ; // comment in input, ignore
        else {
            sscanf(line, "%[ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghjiklmnopqrstuvwxyz]=%i\n", name, &value);
        
            // printf("name = %s, value = %d\n", name, value);

            if (!strcmp(name, "ITERATIONS"))
                params->iterations = value;
            if (!strcmp(name, "LIVE_DANGEROUSLY"))
                params->live_dangerously = value;
            if (!strcmp(name, "KEY"))
                params->key = value;
            if (!strcmp(name, "PERMISSIONS"))
                params->permissions = value;
            if (!strcmp(name, "SHM_SIZE"))
                params->size = value;
                
            name[0] = '\0';
            value = 0;
        }
    }
    
    // printf("iterations = %d\n", params->iterations);
    // printf("danger = %d\n", params->live_dangerously);
    // printf("key = %d\n", params->key);
    // printf("permissions = %o\n", params->permissions);
    // printf("size = %d\n", params->size);
}
