# Python modules
import md5
import os

# 3rd party modules
import sysv_ipc

# Utils for this demo
import utils

utils.say("Oooo 'ello, I'm Mrs. Conclusion!")

params = utils.read_params()

semaphore = sysv_ipc.Semaphore(params["KEY"])
memory = sysv_ipc.SharedMemory(params["KEY"])

utils.say("memory attached at %d" % memory.address)

what_i_wrote = ""
s = ""

for i in xrange(0, params["ITERATIONS"]):
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
        try:
            assert(s == md5.new(what_i_wrote).hexdigest())
        except:
            raise AssertionError, "Shared memory corruption after %d iterations." % i

    what_i_wrote = md5.new(s).hexdigest()

    utils.write_to_memory(memory, what_i_wrote)

    if not params["LIVE_DANGEROUSLY"]:
        utils.say("releasing the semaphore")
        semaphore.release()
