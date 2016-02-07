# Python modules
import time
import md5

# 3rd party modules
import sysv_ipc

# Utils for this demo
import utils

utils.say("Oooo 'ello, I'm Mrs. Premise!")

params = utils.read_params()

# Create the message queue.
mq = sysv_ipc.MessageQueue(params["KEY"], sysv_ipc.IPC_CREX)

# The first message is a random string (the current time).
s = time.asctime()
utils.say("Sending %s" % s)
mq.send(s)
what_i_sent = s

for i in xrange(0, params["ITERATIONS"]):
    utils.say("iteration %d" % i)
    
    s, _ = mq.receive()
    utils.say("Received %s" % s)

    # If the message is what I wrote, put it back on the queue.
    while s == what_i_sent:
        # Nothing new; give Mrs. Conclusion another chance to respond.
        mq.send(s)
        
        s, _ = mq.receive()
        utils.say("Received %s" % s)

    # What I read must be the md5 of what I wrote or something's 
    # gone wrong.
    try:
        assert(s == md5.new(what_i_sent).hexdigest())
    except:
        raise AssertionError, "Message corruption after %d iterations." % i

    # MD5 the reply and write back to Mrs. Conclusion.
    s = md5.new(s).hexdigest()
    utils.say("Sending %s" % s)
    mq.send(s)
    what_i_sent = s

utils.say("")
utils.say("%d iterations complete" % (i + 1))

utils.say("Destroying the message queue.")
mq.remove()
