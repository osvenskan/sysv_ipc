# Python modules
import time
import md5
import os

# 3rd party modules
import sysv_ipc

# Utils for this demo
import utils

utils.say("Oooo 'ello, I'm Mrs. Premise!")

params = utils.read_params()

# Create the semaphore & shared memory. I read somewhere that semaphores 
# and shared memory have separate key spaces, so one can safely use the 
# same key for each. This seems to be true in my experience.

# For purposes of simplicity, this demo code makes no allowance for the 
# failure of the semaphore or memory constructors. This is unrealistic 
# because one can never predict whether or not a given key will be available,
# so your code must *always* be prepared for these functions to fail. 

flags = sysv_ipc.IPC_CREAT | sysv_ipc.IPC_EXCL

semaphore = sysv_ipc.SysVSemaphore(params["KEY"], flags)

memory = sysv_ipc.SysVSharedMemory(params["KEY"], flags)

# I seed the shared memory with a random value which is the current time.
what_i_wrote = time.asctime()
s = what_i_wrote

utils.write_to_memory(memory, what_i_wrote)

for i in xrange(0, params["ITERATIONS"]):
    utils.say("iteration %d" % i)
    if not params["LIVE_DANGEROUSLY"]:
        # Releasing the semaphore...
        utils.say("releasing the semaphore")
        semaphore.release()
        # ...and wait for it to become available again. In real code it'd be 
        # wise to sleep briefly before calling .acquire() in order to be 
        # polite and give other processes an opportunity to grab the semaphore
        # while it is free and thereby avoid starvation. But this code is meant
        # to be a stress test that maximizes the opportunity for shared memory
        # corruption, and politeness has no place in that.
        utils.say("acquiring the semaphore...")
        semaphore.acquire()

    s = utils.read_from_memory(memory)

    # I keep checking the shared memory until something new has been written.
    while s == what_i_wrote:
        if not params["LIVE_DANGEROUSLY"]:
            utils.say("releasing the semaphore")
            semaphore.release()
            utils.say("acquiring the semaphore...")
            semaphore.acquire()

        # Once the call to .acquire() completes, I own the shared resource and
        # I'm free to read from the memory.
        s = utils.read_from_memory(memory)

    # What I read must be the md5 of what I wrote or something's gone wrong.
    try:
        assert(s == md5.new(what_i_wrote).hexdigest())
    except:
        raise AssertionError, "Shared memory corruption after %d iterations." % i

    what_i_wrote = md5.new(s).hexdigest()

    utils.write_to_memory(memory, what_i_wrote)


# Announce for one last time that the semaphore is free again so that 
# Mrs. Conclusion can exit.
if not params["LIVE_DANGEROUSLY"]:
    utils.say("Final release of the semaphore followed by a 5 second pause")
    semaphore.release()
    time.sleep(5)
    # ...before beginning to wait until it is free again.
    utils.say("Final acquisition of the semaphore")
    semaphore.acquire()

utils.say("Destroying semaphore and shared memory")
# It'd be more natural to call memory.remove() and semaphore.remove() here, 
# but I'll use the module-level functions instead to demonstrate their use.
sysv_ipc.remove_shared_memory(memory.id)
sysv_ipc.remove_semaphore(semaphore.id)
