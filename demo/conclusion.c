#include <sys/ipc.h>		/* for system's IPC_xxx definitions */
#include <sys/shm.h>		/* for shmget, shmat, shmdt, shmctl */
#include <sys/sem.h>		/* for semget, semctl, semop */

#include <stdio.h> 
#include <errno.h> 
#include <unistd.h> 
#include <string.h> 
#include <time.h>

#include "md5.h"
#include "utils.h"

static const char MY_NAME[] = "Mrs. Conclusion";

// Set up a Mrs. Premise & Mrs. Conclusion conversation.

int main() { 
    int sem_id = 0;
    int shm_id = 0;
    int rc;
    char s[1024];
    int i;
    int done;
    char last_message_i_wrote[256];
    char md5ified_message[256];
    void *address = NULL;
    struct param_struct params;
    
    say(MY_NAME, "Oooo 'ello, I'm Mrs. Conclusion!");
    
    read_params(&params);

    // Mrs. Premise has already created the semaphore and shared memory. 
    // I just need to get handles to them.
    sem_id = semget(params.key, 0, params.permissions);
    
    if (-1 == sem_id) {
        sem_id = 0;
        sprintf(s, "Getting a handle to the semaphore failed; errno is %d", errno);
        say(MY_NAME, s);
    }
    else {
        // get a handle to the shared memory
        shm_id = shmget(params.key, params.size, params.permissions);
        
        if (shm_id == -1) {
            shm_id = 0;
            sprintf(s, "Couldn't get a handle to the shared memory; errno is %d", errno);
            say(MY_NAME, s);
        }
        else {
            sprintf(s, "Shared memory's id is %d", shm_id);
            say(MY_NAME, s);

            // Attach the memory.
            address = shmat(shm_id, NULL, 0);

            if ((void *)-1 == address) {
                address = NULL;
                sprintf(s, "Attaching the shared memory failed; errno is %d", errno);
                say(MY_NAME, s);
            }
            else {
                sprintf(s, "shared memory address = %p", address);
                say(MY_NAME, s);

                i = 0;
                done = 0;
                last_message_i_wrote[0] = '\0';
                while (!done) {
                    sprintf(s, "iteration %d", i);
                    say(MY_NAME, s);

                    // Wait for Mrs. Premise to free up the semaphore.
                    rc = acquire_semaphore(MY_NAME, sem_id, params.live_dangerously);
                    if (rc)
                        done = 1;
                    else {
                        while ( (!rc) && \
                                (!strcmp((char *)address, last_message_i_wrote)) 
                              ) {
                            // Nothing new; give Mrs. Premise another change to respond.
                            sprintf(s, "Read %zu characters '%s'", strlen((char *)address), (char *)address);
                            say(MY_NAME, s);
                            rc = release_semaphore(MY_NAME, sem_id, params.live_dangerously);
                            if (!rc) {
                                rc = acquire_semaphore(MY_NAME, sem_id, params.live_dangerously);
                            }
                        }
                
                        md5ify(last_message_i_wrote, md5ified_message);

                        // I always accept the first message (when i == 0)
                        if ( (i == 0) || (!strcmp(md5ified_message, (char *)address)) ) {
                            // All is well
                            i++;
            
                            if (i == params.iterations) 
                                done = 1;

                            // MD5 the reply and write back to Mrs. Premise.
                            md5ify((char *)address, md5ified_message);

                            // Write back to Mrs. Premise.
                            sprintf(s, "Writing %zu characters '%s'", strlen(md5ified_message), md5ified_message);
                            say(MY_NAME, s);

                            strcpy((char *)address, md5ified_message);

                            strcpy(last_message_i_wrote, md5ified_message);
                        }
                        else {
                            sprintf(s, "Shared memory corruption after %d iterations.", i);
                            say(MY_NAME, s);                            
                            sprintf(s, "Mismatch; rc = %d, new message is '%s', expected '%s'.", rc, (char *)address, md5ified_message);
                            say(MY_NAME, s);
                            done = 1;
                        }                        
                    }

                    // Release the semaphore.
                    rc = release_semaphore(MY_NAME, sem_id, params.live_dangerously);
                    if (rc)
                        done = 1;
                }

                if (-1 == shmdt(address)) {
                    sprintf(s, "Detaching the memory failed; errno is %d", errno);
                    say(MY_NAME, s);
                }
                address = NULL;
            }
        }
    }
                    
  
    return 0; 
}
