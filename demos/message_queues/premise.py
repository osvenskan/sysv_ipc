#!/usr/bin/env python3

# Python modules
import time
import hashlib

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
utils.say(f"Sending {s}")
mq.send(s)
what_i_sent = s

for i in range(0, params["ITERATIONS"]):
    utils.say(f"iteration {i}")

    s, _ = mq.receive()
    s = s.decode()
    utils.say(f"Received {s}")

    # If the message is what I wrote, put it back on the queue.
    while s == what_i_sent:
        # Nothing new; give Mrs. Conclusion another chance to respond.
        mq.send(s)

        s, _ = mq.receive()
        s = s.decode()
        utils.say(f"Received {s}")

    # What I read must be the md5 of what I wrote or something's
    # gone wrong.
    what_i_sent = what_i_sent.encode()
    try:
        assert(s == hashlib.md5(what_i_sent).hexdigest())
    except AssertionError:
        raise AssertionError(f"Message corruption after {i} iterations.")

    # MD5 the reply and write back to Mrs. Conclusion.
    s = hashlib.md5(s.encode()).hexdigest()
    utils.say(f"Sending {s}")
    mq.send(s)
    what_i_sent = s

utils.say("")
utils.say(f"{i + 1} iterations complete")

utils.say("Destroying the message queue.")
mq.remove()
