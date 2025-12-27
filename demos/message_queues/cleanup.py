#!/usr/bin/env python3

import sysv_ipc
import utils

params = utils.read_params()

key = params["KEY"]

try:
    mq = sysv_ipc.MessageQueue(key)
except sysv_ipc.ExistentialError:
    print(f'''Message queue with key "{key}" doesn't exist.''')
else:
    mq.remove()
    print(f'Message queue with key "{key}" removed')

print("\nAll clean!")
