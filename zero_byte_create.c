#include <sys/ipc.h>            /* for system's IPC_xxx definitions */
#include <sys/shm.h>            /* for shmget, shmat, shmdt, shmctl */
#include <sys/sem.h>            /* for semget, semctl, semop */
#include <stdio.h>
#include <errno.h>

int main (int argc, char const *argv[])
{
int id;

id = shmget(99, 0, 0600 | IPC_CREAT | IPC_EXCL);
if (id == -1) {
   printf("errno = %d\n", errno);

}


}

