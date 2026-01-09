#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

/* This demonstrates the problem described in https://github.com/osvenskan/sysv_ipc/issues/38.

Compile with this command:
    cc -Wall explore_message_queue_receive.c -o explore_message_queue_receive

*/

const int MSG_LEN = 6;		// All of the messages I send are 6 bytes long


void receive_message(int mq_id, long type) {
	struct queue_message {
	    long type;
	    char message[];
	};
	struct queue_message *p_msg = NULL;

	p_msg = (struct queue_message *)malloc(offsetof(struct queue_message, message) + MSG_LEN);
    msgrcv(mq_id, p_msg, (size_t)offsetof(struct queue_message, message) + MSG_LEN, type, 0);

    printf("Received message: ('%s', %ld)\n", p_msg->message, p_msg->type);

    free(p_msg);
}


int main() {
	int i = 0;
	int mq_id = 0;
	struct msqid_ds mq_info;
	struct queue_message {
	    long type;
	    char message[];
	};
	struct queue_message *p_msg = NULL;
	char msg_text[MSG_LEN];

	// Create the queue
	mq_id = msgget(42, IPC_CREAT | IPC_EXCL | 0600);

	/* Place four messages on the queue in this order --
		- ('type4', 4)
		- ('type3', 3)
		- ('type2', 2)
		- ('type1', 1)
	*/
	for (i = 4; i; i--) {
		p_msg = (struct queue_message *)malloc(offsetof(struct queue_message, message) + MSG_LEN);
		sprintf(msg_text, "type%d", i);
		memcpy(p_msg->message, msg_text, MSG_LEN);
		p_msg->type = i;

		msgsnd(mq_id, p_msg, (size_t)MSG_LEN, 0);

		free(p_msg);
	}

	/* Receive the first message, passing a type of -2. The doc for msgrcv() says, "If msgtyp is
	less than 0, the first message of the lowest type that is less than or equal to the absolute
	value of msgtyp shall be received."
	https://pubs.opengroup.org/onlinepubs/9799919799/functions/msgrcv.html

	According to this logic, the message that should be returned is ('type2', 2), and this is what
	I see under Mac OS and FreeBSD. Under Linux, I get ('type1', 1) which I believe is incorrect.
	*/
	receive_message(mq_id, -2);

    /* Pull the remaining messages from the queue in order. They should be FIFO --
		- ('type4', 4)
		- ('type3', 3)
		- ('type1', 1)
	*/
	for (i = 0; i < 3; i++)
		receive_message(mq_id, 0);

	if (-1 == msgctl(mq_id, IPC_RMID, &mq_info))
		printf("msgctl returned -1 on id %d, errno = %d\n", mq_id, errno);

	return 0;
}
