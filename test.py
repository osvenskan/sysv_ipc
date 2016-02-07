import sysv_ipc as svi
import pdb

# f = open("x.txt")
# for line in f.readlines():
#     i = int(line.strip())
#     if (i < 0) or (i > 2147483647):
#         print line



#mq = svi.MessageQueue(99, svi.IPC_CREX)
#mq = svi.MessageQueue(None, svi.IPC_CREX)

#sem = svi.Semaphore(99, 0, 0600, 1)
#sem = svi.Semaphore(None, svi.IPC_CREAT|svi.IPC_EXCL, 0600, 1)
#sem = svi.Semaphore(99, svi.IPC_CREAT)

#mem = svi.SharedMemory(99, svi.IPC_CREAT|svi.IPC_EXCL, 0600, size=20)
mem = svi.SharedMemory(None, svi.IPC_CREX)

print "memory id = {}".format(mem.id)

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

mem.write("kha", 10)

#mem.detach()

#mem.remove()


#mem = mem

#pdb.set_trace()

#mq.send("iahsdgl", type=42)

#pdb.set_trace()
# 
# print mq.receive()

#mem2 = svi.SharedMemory(99)

#print mem2.size

# mem=mem
# 
# mem.detach()
# mem.remove()

i = 42

# sem = sem
# sem.remove()


#mq.remove()

