import sysv_ipc
import time
import sys
import random

# The parent passes the semaphore's name to me.
key = sys.argv[1]

sem = sysv_ipc.Semaphore(int(key))

print('Child: waiting to aquire semaphore ' + key)

with sem:
    print('Child: semaphore {} aquired; holding for 3 seconds.'.format(sem.key))

    # Flip a coin to determine whether or not to bail out of the context.
    if random.randint(0, 1):
        print("Child: raising ValueError to demonstrate unplanned context exit")
        raise ValueError

    time.sleep(3)

    print('Child: gracefully exiting context (releasing the semaphore).')
