import sysv_ipc as svi
import pdb



#sem = svi.SysVSemaphore(99, svi.IPC_CREAT|svi.IPC_EXCL, 0600, 1)
#sem = svi.SysVSemaphore(99, svi.IPC_CREAT)

#mem = svi.SysVSharedMemory(99, svi.IPC_CREAT|svi.IPC_EXCL, 0600, size=20)
mem = svi.SysVSharedMemory(99, svi.IPC_CREX, 0600)

# 
# value = sem.value
# 
# while value == sem.value:
#     value += 1
#     sem.release()
#     
#     print sem.value
# 
# 

#mem.read(offset=-1)


mem = mem

pdb.set_trace()


mem2 = svi.SysVSharedMemory(99)

print mem2.size

mem.detach()
mem.remove()

# sem = sem
# sem.remove()