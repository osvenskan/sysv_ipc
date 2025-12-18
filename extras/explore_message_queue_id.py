import sysv_ipc

def main():
    '''This explores what happens to message queue ids when a large number of queues are created.

    The POSIX spec says "msgget() shall return a non-negative integer" on success, but MacOS does
    not respect this. As of this writing (December 2025) and for a long time prior, it has been
    possible to get a negative queue id from msgget(). I strongly suspect that it's because
    (a) MacOS (apparently) generates queue ids serially by adding a fixed value (e.g. 65,536)
    to the previous queue id, and (b) that code fails to realize when it has exceeded the maximum
    positive value of an int (2,147,483,648). It adds the fixed value to something that's just
    short of INT_MAX which flips the sign bit and returns a negative number.

    This is demonstrated by the code below. It allocates and destroys message queues until it hits
    a negative one (or completes its max number of attempts). As of this writing, for me, this
    reaches returns a negative queue id 100% of the time, and takes < 1 second to run.

    msgget() ref: https://pubs.opengroup.org/onlinepubs/9799919799/functions/msgget.html
    '''
    MAX_ITERATIONS = 200000
    done = False
    i = 0

    while not done:
        mq = sysv_ipc.MessageQueue(None, flags=sysv_ipc.IPC_CREX)

        print(mq.id)

        done = (mq.id < 0)

        mq.remove()

        done |= (i == MAX_ITERATIONS)

        i += 1

if __name__ =='__main__':
    main()
