import sysv_ipc
import utils

params = utils.read_params()

key = params["KEY"]

try:
    mq = sysv_ipc.MessageQueue(key)
except sysv_ipc.ExistentialError:
    print('''Message queue with key "{}" doesn't exist.'''.format(key))
else:
    mq.remove()
    print('Message queue with key "{}" removed'.format(key))

print("\nAll clean!")
