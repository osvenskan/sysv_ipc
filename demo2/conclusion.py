# Python modules
import time
import md5

# 3rd party modules
import sysv_ipc

# Utils for this demo
import utils

params = utils.read_params()

# Mrs. Premise has already created the message queue. I just need a handle
# to it.
mq = sysv_ipc.MessageQueue(params["KEY"])

what_i_sent = ""

for i in xrange(0, params["ITERATIONS"]):
    utils.say("iteration %d" % i)

    s, _ = mq.receive()
    utils.say("Received %s" % s)

    while s == what_i_sent:
        # Nothing new; give Mrs. Premise another chance to respond.
        mq.send(s)
        
        s, _ = mq.receive()
        utils.say("Received %s" % s)

    if what_i_sent:
        try:
            assert(s == md5.new(what_i_sent).hexdigest())
        except:
            raise AssertionError, "Message corruption after %d iterations." % i
    #else:
        # When what_i_sent is blank, this is the first message which
        # I always accept without question.

    # MD5 the reply and write back to Mrs. Premise.
    s = md5.new(s).hexdigest()
    utils.say("Sending %s" % s)
    mq.send(s)
    what_i_sent = s
    

utils.say("")
utils.say("%d iterations complete" % (i + 1))
