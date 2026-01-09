#include <errno.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>

static const int MAX_ITERATIONS = 100000;

/* This explores what happens to message queue ids when a large number of queues are created.

Compile with this command:
    cc -Wall explore_message_queue_id.c -o explore_message_queue_id

Details: https://github.com/osvenskan/sysv_ipc/issues/63
*/

int main() {
	int i = 0;
	int done = 0;
	int mq_id = 0;
	struct msqid_ds mq_info;

	while (!done) {
		i += 1;

		mq_id = msgget(42, IPC_CREAT | IPC_EXCL | 0600);

		printf("%05d: queue id is %d\n", i, mq_id);

		if ((mq_id < 0) || (i == MAX_ITERATIONS))
			done = 1;

		if (-1 == msgctl(mq_id, IPC_RMID, &mq_info))
			printf("msgctl returned -1 on id %d, errno = %d\n", mq_id, errno);
	}

	return 0;
}
