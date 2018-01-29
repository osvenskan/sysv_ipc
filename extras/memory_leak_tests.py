# Python modules
import gc
import os
import subprocess
import random
import re
import sys

# My module
import sysv_ipc


SKIP_SEMAPHORE_TESTS = False
SKIP_SHARED_MEMORY_TESTS = False
SKIP_MESSAGE_QUEUE_TESTS = False


# TEST_COUNT = 10
TEST_COUNT = 1024 * 100

PY_MAJOR_VERSION = sys.version_info[0]

# ps output looks like this:
#   RSZ      VSZ
#   944    75964
ps_output_regex = re.compile("""
    ^
    \s*   # whitespace before first heading
    \S*   # first heading (e.g. RSZ)
    \s+   # whitespace between headings
    \S*   # second heading (e.g VSZ)
    \s+   # newline and whitespace before first numeric value
    (\d+) # first value
    \s+   # whitespace between values
    (\d+) # second value
    \s*   # trailing whitespace if any
    $
""", re.MULTILINE | re.VERBOSE)

# On OS X, Ubuntu and OpenSolaris, both create/destroy tests show some growth
# is rsz and vsz. (e.g. 3248 versus 3240 -- I guess these are measured
# in kilobytes?) When I increased the number of iterations by a factor of 10,
# the delta didn't change which makes me think it isn't an actual leak
# but just some memory consumed under normal circumstances.


def random_string(length):
    return ''.join(random.sample("abcdefghijklmnopqrstuvwxyz", length))


def print_mem_before():
    s = "Memory usage before, RSS = %d, VSZ = %d" % get_memory_usage()
    print(s)


def print_mem_after():
    gc.collect()

    if gc.garbage:
        s = "Leftover garbage:" + str(gc.garbage)
        print(s)
    else:
        print("Python's GC reports no leftover garbage")

    s = "Memory usage after, RSS = %d, VSZ = %d" % get_memory_usage()
    print(s)


def get_memory_usage():
    # `ps` has lots of format options that vary from OS to OS, and some of
    # those options have aliases (e.g. vsz, vsize). The ones I use below
    # appear to be the most portable.
    s = subprocess.Popen(["ps", "-p", str(os.getpid()), "-o", "rss,vsz"],
                         stdout=subprocess.PIPE).communicate()[0]

    # Output looks like this:
    #   RSZ      VSZ
    #   944    75964

    if PY_MAJOR_VERSION > 2:
        s = s.decode(sys.getfilesystemencoding())

    m = ps_output_regex.match(s)

    rsz = int(m.groups()[0])
    vsz = int(m.groups()[1])

    return rsz, vsz


# Assert manual control over the garbage collector
gc.disable()

if SKIP_SEMAPHORE_TESTS:
    print("Skipping semaphore tests")
else:
    print("Running semaphore create/destroy test...")
    print_mem_before()

    for i in range(1, TEST_COUNT):
        sem = sysv_ipc.Semaphore(None, sysv_ipc.IPC_CREX)
        sem.remove()

    print_mem_after()

    print("Running semaphore acquire/release test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        sem.release()
        sem.acquire()

    sem.remove()

    print_mem_after()

    print("Running semaphore Z test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        sem.Z()

    sem.remove()

    print_mem_after()

    if sysv_ipc.SEMAPHORE_TIMEOUT_SUPPORTED:
        print("Running semaphore acquire timeout test...")
        print_mem_before()

        sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

        for i in range(1, TEST_COUNT):
            try:
                sem.acquire(.001)
            except sysv_ipc.BusyError:
                pass

        sem.remove()

        print_mem_after()
    else:
        print("Skipping semaphore acquire timeout test (not supported on this platform)")

    if sysv_ipc.SEMAPHORE_TIMEOUT_SUPPORTED:
        print("Running semaphore Z timeout test...")
        print_mem_before()

        sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

        # Release the semaphore to make the value is non-zero so that .Z()
        # has to wait for the timeout.
        sem.release()

        for i in range(1, TEST_COUNT):
            try:
                sem.Z(.001)
            except sysv_ipc.BusyError:
                pass

        sem.remove()

        print_mem_after()
    else:
        print("Skipping semaphore Z timeout test (not supported on this platform)")

    print("Running semaphore key read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.key

    sem.remove()

    print_mem_after()

    print("Running semaphore id read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.id

    sem.remove()

    print_mem_after()

    print("Running semaphore value read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.value

    sem.remove()

    print_mem_after()

    print("Running semaphore value write test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        value = random.randint(0, sysv_ipc.SEMAPHORE_VALUE_MAX)
        sem.value = value
        assert(sem.value == value)

    sem.remove()

    print_mem_after()

    print("Running semaphore undo read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.undo

    sem.remove()

    print_mem_after()

    print("Running semaphore undo write test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        undo = random.randint(0, 1)
        sem.undo = undo
        assert(sem.undo == undo)

    sem.remove()

    print_mem_after()

    print("Running semaphore block read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.block

    sem.remove()

    print_mem_after()

    print("Running semaphore block write test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        block = random.randint(0, 1)
        sem.block = block
        assert(sem.block == block)

    sem.remove()

    print_mem_after()

    print("Running semaphore mode read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.mode

    sem.remove()

    print_mem_after()

    print("Running semaphore mode write test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        # octal 600 = decimal 384
        sem.mode = 384

    sem.remove()

    print_mem_after()

    print("Running semaphore uid read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.uid

    sem.remove()

    print_mem_after()

    print("Running semaphore uid write test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    uid = sem.uid

    for i in range(1, TEST_COUNT):
        sem.uid = uid

    sem.remove()

    print_mem_after()

    print("Running semaphore gid read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.gid

    sem.remove()

    print_mem_after()

    print("Running semaphore gid write test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    gid = sem.gid

    for i in range(1, TEST_COUNT):
        sem.gid = gid

    sem.remove()

    print_mem_after()

    print("Running semaphore cuid read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.cuid

    sem.remove()

    print_mem_after()

    print("Running semaphore cgid read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.cgid

    sem.remove()

    print_mem_after()

    print("Running semaphore last_pid read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.last_pid

    sem.remove()

    print_mem_after()

    print("Running semaphore waiting_for_nonzero read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.waiting_for_nonzero

    sem.remove()

    print_mem_after()

    print("Running semaphore waiting_for_zero read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.waiting_for_zero

    sem.remove()

    print_mem_after()

    print("Running semaphore o_time read test...")
    print_mem_before()

    sem = sysv_ipc.Semaphore(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = sem.o_time

    sem.remove()

    print_mem_after()

# ============== Memory tests ==============

if SKIP_SHARED_MEMORY_TESTS:
    print("Skipping shared memory tests")
else:
    print("Running memory create/destroy test...")
    print_mem_before()

    init_character = 'z'.encode()

    init_character_toggle = True
    for i in range(1, TEST_COUNT):
        # Test with and w/o init character
        if init_character_toggle:

            mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX, size=sysv_ipc.PAGE_SIZE,
                                        init_character=init_character)
        else:
            mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX, size=sysv_ipc.PAGE_SIZE)

        init_character_toggle = not init_character_toggle

        mem.detach()

        mem.remove()

    print_mem_after()

    print("Running memory read/write test with strings...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX, size=sysv_ipc.PAGE_SIZE)
    alphabet = "abcdefghijklmnopqrstuvwxyz"

    s = alphabet
    length = len(s)
    for i in range(1, TEST_COUNT):
        # length = random.randint(1, sysv_ipc.PAGE_SIZE)
        # s = ''.join( [ random.choice(alphabet) for j in range(1, length + 1) ] )
        mem.write(s)
        assert(s == mem.read(length).decode())

    mem.detach()

    mem.remove()

    print_mem_after()

    if PY_MAJOR_VERSION > 2:
        print("Running memory read/write test with bytes...")
        print_mem_before()

        mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX, size=sysv_ipc.PAGE_SIZE)
        alphabet = "abcdefghijklmnopqrstuvwxyz".encode()

        s = alphabet
        length = len(s)
        for i in range(1, TEST_COUNT):
            # length = random.randint(1, sysv_ipc.PAGE_SIZE)
            # s = ''.join( [ random.choice(alphabet) for j in range(1, length + 1) ] )
            mem.write(s)
            assert(s == mem.read(length))

        mem.detach()

        mem.remove()

        print_mem_after()

    print("Running memory key read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.key

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory id read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.id

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory size read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.size

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory address read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.address

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory attached read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.attached

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory last_attach_time test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.last_attach_time

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory last_detach_time test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.last_detach_time

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory last_change_time test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.last_change_time

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory creator_pid test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.creator_pid

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory last_pid test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.last_pid

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory number_attached test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.number_attached

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory mode read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.mode

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory mode write test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        # octal 600 = decimal 384
        mem.mode = 384

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory uid read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.uid

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory uid write test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    uid = mem.uid

    for i in range(1, TEST_COUNT):
        mem.uid = uid

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory gid read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.gid

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory gid write test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    gid = mem.gid

    for i in range(1, TEST_COUNT):
        mem.gid = gid

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory cuid read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.cuid

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory cgid read test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mem.cgid

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory create bytearray test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = bytearray(mem)

    mem.detach()
    mem.remove()

    print_mem_after()

    print("Running memory create memoryview test...")
    print_mem_before()

    mem = sysv_ipc.SharedMemory(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = memoryview(mem)

    mem.detach()
    mem.remove()

    print_mem_after()

# ================ Message queue tests  ==============


if SKIP_MESSAGE_QUEUE_TESTS:
    print("Skipping Message queue tests")
else:
    print("Running Message queue create/destroy test...")
    print_mem_before()

    for i in range(1, TEST_COUNT):
        mq = sysv_ipc.MessageQueue(None, sysv_ipc.IPC_CREX)
        mq.remove()

    print_mem_after()

    print("Running message queue send/receive test with strings...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        s = random_string(15)
        mq.send(s)
        assert(s.encode() == mq.receive()[0])

    mq.remove()

    print_mem_after()

    if PY_MAJOR_VERSION > 2:
        print("Running message queue send/receive test with bytes...")
        print_mem_before()

        mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

        for i in range(1, TEST_COUNT):
            s = random_string(15).encode()
            mq.send(s)
            assert(s == mq.receive()[0])

        mq.remove()

        print_mem_after()

    print("Running message queue key read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.key

    mq.remove()

    print_mem_after()

    print("Running  message queue id read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.id

    mq.remove()

    print_mem_after()

    print("Running message queue max size read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.max_size

    mq.remove()

    print_mem_after()

    print("Running message queue last_send_time read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.last_send_time

    mq.remove()

    print_mem_after()

    print("Running message queue last_receive_time read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.last_receive_time

    mq.remove()

    print_mem_after()

    print("Running message queue last_change_time read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.last_change_time

    mq.remove()

    print_mem_after()

    print("Running message queue last_send_pid read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.last_send_pid

    mq.remove()

    print_mem_after()

    print("Running message queue last_receive_pid read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.last_receive_pid

    mq.remove()

    print_mem_after()

    print("Running message queue current_messages read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.current_messages

    mq.remove()

    print_mem_after()

    print("Running message queue uid read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.uid

    mq.remove()

    print_mem_after()

    print("Running message queue uid write test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    uid = mq.uid

    for i in range(1, TEST_COUNT):
        mq.uid = uid

    mq.remove()

    print_mem_after()

    print("Running message queue gid read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.gid

    mq.remove()

    print_mem_after()

    print("Running message queue gid write test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    gid = mq.gid

    for i in range(1, TEST_COUNT):
        mq.gid = gid

    mq.remove()

    print_mem_after()

    print("Running message queue cuid read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.cuid

    mq.remove()

    print_mem_after()

    print("Running message queue cgid read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.cgid

    mq.remove()

    print_mem_after()

    print("Running message queue mode read test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        foo = mq.mode

    mq.remove()

    print_mem_after()

    print("Running message queue mode write test...")
    print_mem_before()

    mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)

    for i in range(1, TEST_COUNT):
        # octal 600 = decimal 384
        mq.mode = 384

    mq.remove()

    print_mem_after()
