import sysv_ipc

done = False

size = 1024

while not done:
    s = "Trying %d (%dk)..." % (size, size / 1024)
    print(s)
    try:
        mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX, size=size)
    except MemoryError:
        done = True
    else:
        mem.detach()
        mem.remove()
        size += 1024
