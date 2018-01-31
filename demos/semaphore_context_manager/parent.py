import subprocess
import time
import os

import sysv_ipc

sem = sysv_ipc.Semaphore(None, sysv_ipc.IPC_CREX, initial_value=1)
print("Parent: created semaphore {}.".format(sem.key))

sem.acquire()

# Spawn a child that will wait on this semaphore.
path, _ = os.path.split(__file__)
print("Parent: spawning child process...")
subprocess.Popen(["python", os.path.join(path, 'child.py'), str(sem.key)])

for i in range(3, 0, -1):
    print("Parent: child process will acquire the semaphore in {} seconds...".format(i))
    time.sleep(1)

sem.release()

# Sleep for a second to give the child a chance to acquire the semaphore.
# This technique is a little sloppy because technically the child could still
# starve, but it's certainly sufficient for this demo.
time.sleep(1)

# Wait for the child to release the semaphore.
print("Parent: waiting for the child to release the semaphore.")
sem.acquire()

# Clean up.
print("Parent: destroying the semaphore.")
sem.release()
sem.remove()

msg = """
By the time you're done reading this, the parent will have exited and so the
operating system will have destroyed the semaphore. You can prove that the
semaphore is gone by running this command and observing that it raises
sysv_ipc.ExistentialError --

   python -c "import sysv_ipc; sysv_ipc.Semaphore({})"

""".format(sem.key)

print(msg)
