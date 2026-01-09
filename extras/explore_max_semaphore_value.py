import sysv_ipc

'''This is a simple test to see how many times a semaphore can be released.'''

# While POSIX semaphores sometimes have a max value of 2^31 (2,147,483,648), I don't think I've
# seen SysV semaphores with a max value > 2^15 (32,768), with the exception of MacOS which I
# believe is just buggy. See https://github.com/osvenskan/sysv_ipc/issues/62
N_ATTEMPTS = 100000

sem = sysv_ipc.Semaphore(None, sysv_ipc.IPC_CREX)

print(f'Semaphore key is {sem.key}')

done = False
i = 0

while not done:
    try:
        sem.release()
    except ValueError:
        print('Done. Max semaphore value has been reached.')
        done = True
    else:
        i += 1
        print(f'{i:05}: value is {sem.value}')

        if i == N_ATTEMPTS:
            done = True
            print(f'Done. Max number of attempts ({N_ATTEMPTS}) has been reached.')

sem.remove()
