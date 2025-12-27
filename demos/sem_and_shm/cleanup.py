#!/usr/bin/env python3

# 3rd party modules
import sysv_ipc

# Modules for this project
import utils

params = utils.read_params()

key = params["KEY"]

try:
    semaphore = sysv_ipc.Semaphore(key)
except sysv_ipc.ExistentialError:
    print(f'''The semaphore with key "{key}" doesn't exist.''')
else:
    semaphore.remove()
    print(f'Removed the semaphore with key "{key}".')


try:
    memory = sysv_ipc.SharedMemory(key)
except sysv_ipc.ExistentialError:
    print(f'''The shared memory with key "{key}" doesn't exist.''')
else:
    memory.remove()
    print(f'Removed the shared memory with key "{key}".')
