# Python modules
import sys
import hashlib

# 3rd party modules
import sysv_ipc

# Utils for this demo
import utils

PY_MAJOR_VERSION = sys.version_info[0]

utils.say("Oooo 'ello, I'm Mrs. Conclusion!")

params = utils.read_params()

semaphore = sysv_ipc.Semaphore(params["KEY"])
memory = sysv_ipc.SharedMemory(params["KEY"])

utils.say("memory attached at %d" % memory.address)

what_i_wrote = ""
s = ""

for i in range(0, params["ITERATIONS"]):
    utils.say("i = %d" % i)
    if not params["LIVE_DANGEROUSLY"]:
        # Wait for Mrs. Premise to free up the semaphore.
        utils.say("acquiring the semaphore...")
        semaphore.acquire()

    s = utils.read_from_memory(memory)

    while s == what_i_wrote:
        if not params["LIVE_DANGEROUSLY"]:
            # Release the semaphore...
            utils.say("releasing the semaphore")
            semaphore.release()
            # ...and wait for it to become available again.
            utils.say("acquiring for the semaphore...")
            semaphore.acquire()

        s = utils.read_from_memory(memory)

    if what_i_wrote:
        if PY_MAJOR_VERSION > 2:
            what_i_wrote = what_i_wrote.encode()
        try:
            assert(s == hashlib.md5(what_i_wrote).hexdigest())
        except AssertionError:
            raise AssertionError("Shared memory corruption after %d iterations." % i)

    if PY_MAJOR_VERSION > 2:
        s = s.encode()
    what_i_wrote = hashlib.md5(s).hexdigest()

    utils.write_to_memory(memory, what_i_wrote)

    if not params["LIVE_DANGEROUSLY"]:
        utils.say("releasing the semaphore")
        semaphore.release()
