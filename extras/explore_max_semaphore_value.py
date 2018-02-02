import sysv_ipc

'''This is a simple test to see how many times a semaphore can be released.'''

sem = sysv_ipc.Semaphore(None, sysv_ipc.IPC_CREX)

print('Semaphore key is {}'.format(sem.key))

for i in range(1, 100000):
    sem.release()

    print('{:05}: value is {}'.format(i, sem.value))

sem.remove()