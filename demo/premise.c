#include <sys/ipc.h>		/* for system's IPC_xxx definitions */
#include <sys/shm.h>		/* for shmget, shmat, shmdt, shmctl */
#include <sys/sem.h>		/* for semget, semctl, semop */

#include <stdlib.h> 
#include <stdio.h> 
#include <errno.h> 
#include <unistd.h> 
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>

#include "md5.h"
#include "utils.h"

const char MY_NAME[] = "Mrs. Premise";

// Set up a Mrs. Premise & Mrs. Conclusion conversation.

void get_current_time(char *);

int main() { 
    int rc;
    char s[1024];
    char last_message_i_wrote[256];
    char md5ified_message[256];
    int i = 0;
    int done = 0;
    struct param_struct params;
    int shm_id;
    void *address = NULL;
    int sem_id;
    struct shmid_ds shm_info;

    say(MY_NAME, "Oooo 'ello, I'm Mrs. Premise!");
    
    read_params(&params);
    
    // Create the shared memory
    shm_id = shmget(params.key, params.size, IPC_CREAT | IPC_EXCL | params.permissions);
    
    if (shm_id == -1) {
        shm_id = 0;
        sprintf(s, "Creating the shared memory failed; errno is %d", errno);
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
        }
    }
    
    if (address) {
        // Create the semaphore
        sem_id = semget(params.key, 1, IPC_CREAT | IPC_EXCL | params.permissions);
    
        if (-1 == sem_id) {
            sem_id = 0;
            sprintf(s, "Creating the semaphore failed; errno is %d", errno);
            say(MY_NAME, s);
        }
        else {
            sprintf(s, "the semaphore id is %d", sem_id);
            say(MY_NAME, s);
        
            // I seed the shared memory with a random string (the current time).
            get_current_time(s);
    
            strcpy((char *)address, s);
            strcpy(last_message_i_wrote, s);

            sprintf(s, "Wrote %zu characters: %s", strlen(last_message_i_wrote), last_message_i_wrote);
            say(MY_NAME, s);
            
            i = 0;
            while (!done) {
                sprintf(s, "iteration %d", i);
                say(MY_NAME, s);

                // Release the semaphore...
                rc = release_semaphore(MY_NAME, sem_id, params.live_dangerously);
                // ...and wait for it to become available again. In real code 
                // I might want to sleep briefly before calling .acquire() in
                // order to politely give other processes an opportunity to grab
                // the semaphore while it is free so as to avoid starvation. But 
                // this code is meant to be a stress test that maximizes the 
                // opportunity for shared memory corruption and politeness is 
                // not helpful in stress tests.
                if (!rc)
                    rc = acquire_semaphore(MY_NAME, sem_id, params.live_dangerously);

                if (rc)
                    done = 1;
                else {
                    // I keep checking the shared memory until something new has 
                    // been written.
                    while ( (!rc) && \
                            (!strcmp((char *)address, last_message_i_wrote)) 
                          ) {
                        // Nothing new; give Mrs. Conclusion another change to respond.
                        sprintf(s, "Read %zu characters '%s'", strlen((char *)address), (char *)address);
                        say(MY_NAME, s);
                        rc = release_semaphore(MY_NAME, sem_id, params.live_dangerously);
                        if (!rc) {
                            rc = acquire_semaphore(MY_NAME, sem_id, params.live_dangerously);
                        }
                    }


                    if (rc) 
                        done = 1;
                    else {
                        sprintf(s, "Read %zu characters '%s'", strlen((char *)address), (char *)address);
                        say(MY_NAME, s);

                        // What I read must be the md5 of what I wrote or something's 
                        // gone wrong.
                        md5ify(last_message_i_wrote, md5ified_message);
                    
                        if (strcmp(md5ified_message, (char *)address) == 0) {
                            // Yes, the message is OK
                            i++;
                            if (i == params.iterations)
                                done = 1;

                            // MD5 the reply and write back to Mrs. Conclusion.
                            md5ify(md5ified_message, md5ified_message);
                            
                            sprintf(s, "Writing %zu characters '%s'", strlen(md5ified_message), md5ified_message);
                            say(MY_NAME, s);

                            strcpy((char *)address, md5ified_message);
                            strcpy((char *)last_message_i_wrote, md5ified_message);
                        }
                        else {
                            sprintf(s, "Shared memory corruption after %d iterations.", i);
                            say(MY_NAME, s);                            
                            sprintf(s, "Mismatch; new message is '%s', expected '%s'.", (char *)address, md5ified_message);
                            say(MY_NAME, s);
                            done = 1;
                        }
                    }
                }
            }

            // Announce for one last time that the semaphore is free again so that 
            // Mrs. Conclusion can exit.
            say(MY_NAME, "Final release of the semaphore followed by a 5 second pause");            
            rc = release_semaphore(MY_NAME, sem_id, params.live_dangerously);
            sleep(5);
            // ...before beginning to wait until it is free again. 
            // Technically, this is bad practice. It's possible that on a 
            // heavily loaded machine, Mrs. Conclusion wouldn't get a chance
            // to acquire the semaphore. There really ought to be a loop here
            // that waits for some sort of goodbye message but for purposes of
            // simplicity I'm skipping that.

            say(MY_NAME, "Final wait to acquire the semaphore");
            rc = acquire_semaphore(MY_NAME, sem_id, params.live_dangerously);
            if (!rc) {
                say(MY_NAME, "Destroying the shared memory.");
                
                if (-1 == shmdt(address)) {
                    sprintf(s, "Detaching the memory failed; errno is %d", errno);
                    say(MY_NAME, s);
                }
                address = NULL;
                
            
                if (-1 == shmctl(shm_id, IPC_RMID, &shm_info)) {
                    sprintf(s, "Removing the memory failed; errno is %d", errno);
                    say(MY_NAME, s);
                }
            }
        }

        say(MY_NAME, "Destroying the semaphore.");
        // Clean up the semaphore
        if (-1 == semctl(sem_id, 0, IPC_RMID)) {
            sprintf(s, "Removing the semaphore failed; errno is %d", errno);
            say(MY_NAME, s);
        }
    }
    return 0; 
}


void get_current_time(char *s) {
    time_t the_time;
    struct tm *the_localtime;
    char *pAscTime;

    the_time = time(NULL);
    the_localtime = localtime(&the_time);
    pAscTime = asctime(the_localtime);
    
    strcpy(s, pAscTime);
}
