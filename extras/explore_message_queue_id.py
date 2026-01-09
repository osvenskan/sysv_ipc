import sysv_ipc

def main():
    '''This explores what happens to message queue ids when a large number of queues are created
    and destroyed.

    Details: https://github.com/osvenskan/sysv_ipc/issues/63
    '''
    MAX_ITERATIONS = 200000
    done = False
    i = 0

    while not done:
        i += 1

        mq = sysv_ipc.MessageQueue(None, flags=sysv_ipc.IPC_CREX)

        print(f'{i:05}: queue id is {mq.id}')

        if mq.id < 0:
            print('Done. Negative queue id returned.')
            done = True

        if i == MAX_ITERATIONS:
            done = True
            print(f'Done. Max number of attempts ({MAX_ITERATIONS}) has been reached.')

        mq.remove()

if __name__ =='__main__':
    main()
