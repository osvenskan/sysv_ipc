# 3rd party modules
import sysv_ipc

# Modules for this project
import utils

params = utils.read_params()

key = params["KEY"]

try:
    semaphore = sysv_ipc.Semaphore(key)
except sysv_ipc.ExistentialError:
    print('''The semaphore with key "{}" doesn't exist.'''.format(key))
else:
    semaphore.remove()
    print('Removed the semaphore with key "{}".'.format(key))


try:
    memory = sysv_ipc.SharedMemory(key)
except sysv_ipc.ExistentialError:
    print('''The shared memory with key "{}" doesn't exist.'''.format(key))
else:
    memory.remove()
    print('Removed the shared memory with key "{}".'.format(key))
