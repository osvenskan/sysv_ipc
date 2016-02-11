# 3rd party modules
import sysv_ipc

# Modules for this project
import utils

params = utils.read_params()


try:
    semaphore = sysv_ipc.Semaphore(params["KEY"], 0)
except:
    semaphore = None
    
if semaphore:
    semaphore.remove()
    
print ("The semaphore is cleaned up.")
    
    
try:
    memory = sysv_ipc.SharedMemory(params["KEY"], 0)
except:
    memory = None

if memory:
    memory.remove()

print ("The shared memory is cleaned up.")
