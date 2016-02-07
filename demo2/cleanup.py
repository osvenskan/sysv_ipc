import sysv_ipc
import utils

params = utils.read_params()


try:
    mq = sysv_ipc.MessageQueue(params["KEY"])
    mq.remove()
    print "message queue %d removed" % params["KEY"]
except:
    print "message queue doesn't exist"
    


print "\nAll clean!"