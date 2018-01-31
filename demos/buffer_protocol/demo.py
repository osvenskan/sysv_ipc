import sys

import sysv_ipc

IS_PY3 = (sys.version_info[0] == 3)
to_chr = lambda c: chr(c) if IS_PY3 else c  # noqa E731  (silence flake8)
to_ord = lambda c: ord(c) if IS_PY3 else c  # noqa E731  (silence flake8)

# Create a shared memory segment and write the (English) alphabet to the shared memory.
mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX, size=sysv_ipc.PAGE_SIZE)

ASCII_A = 0x61
alphabet = ''.join([chr(ASCII_A + i) for i in range(26)])
if IS_PY3:
    alphabet = bytes(alphabet, 'ASCII')

mem.write(alphabet)

# Create a bytearray from the SharedMemory.
ba = bytearray(mem)

# bytearray instances have "most of the usual methods of mutable sequences", such as replace.
# https://docs.python.org/3/library/functions.html#func-bytearray
ba = ba.replace(b'c', b'x')

assert(ba[:4] == b'abxd')

# Unlike a memoryview (see below), changes to the bytearray do *not* affect the underlying
# SharedMemory -- the bytearray is a copy.
assert(mem.read(4) == b'abcd')

# Reset the memory to contain the alphabet unmodified.
mem.write(alphabet)

# Create a memoryview from the SharedMemory.
mv = memoryview(mem)

# This memoryview has format = 'B', itemsize = 1, shape = (sysv_ipc.PAGE_SIZE, ), ndim = 1,
# strides = (1, ), and is read/write.

# This shows that you can take slices of a memoryview
assert([to_chr(c) for c in mv[3:6]] == ['d', 'e', 'f'])

# This shows that you can write to the memoryview.
mv[4] = to_ord('x')

assert([to_chr(c) for c in mv[3:6]] == ['d', 'x', 'f'])

# Changes to the underlying segment are reflected in the memoryview
mem.write(b'xxx')
assert([to_chr(c) for c in mv[:6]] == ['x', 'x', 'x', 'd', 'x', 'f'])

mem.detach()
mem.remove()

print('Done!')
