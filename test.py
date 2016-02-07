import sysv_ipc as svi
import pdb


sem = svi.SysVSemaphore(99, svi.IPC_CREAT|svi.IPC_EXCL, 0600, 1)
#sem = svi.SysVSemaphore(99, svi.IPC_CREAT)

#shm = svi.SysVSharedMemory(99, svi.IPC_CREAT|svi.IPC_EXCL, 0600, svi.PAGE_SIZE)

pdb.set_trace()

value = sem.value

while value == sem.value:
    value += 1
    sem.release()
    
    print sem.value



#shm.read(offset=-1)


#shm = shm

#shm.remove()

sem = sem
sem.remove()