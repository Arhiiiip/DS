#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "ipc.h"
#include "thread.h"

int send(void *self, local_id dst, const Message *msg) {
    int8_t id = *(int8_t *) self;

    if (write(pipes[id][dst].fd[WRITE], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) == -1) {
        printf("Error with sending from %d to %d\n", id, dst);
        return -1;
    } else {
        printf("Message from %d to %d, type: %s, length: %d has sent\n",
               id, dst, msg->s_header.s_type == 0 ? "STARTED" : "DONE", msg->s_header.s_payload_len);
    }
    return 0;
}

int send_multicast(void *self, const Message *msg) {
    int8_t id = *(int8_t *) self;
    for (int i = 0; i <= count_of_processes; i++) {
        if (i == id) continue;
        if (send(self, (int8_t) i, msg) == -1) {
            printf("Error with sending from %d to %d\n", id, i);
            return -1; // потом посмотрим
        }
    }
    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    int8_t id = *(int8_t *) self;
    MessageHeader message_header;
    char buffer[MAX_PAYLOAD_LEN];
    int size_of_msg = read(pipes[from][id].fd[READ], &message_header, sizeof(MessageHeader));
    if (size_of_msg == -1) {
        printf("Error with reading message from %d to %d", from, id);
        return -1;
    }

    ssize_t amount_of_payload_msg = read(pipes[from][id].fd[READ], buffer, message_header.s_payload_len);
    strncpy(msg->s_payload, buffer, amount_of_payload_msg);
    msg->s_header = message_header;
    int16_t timestamp = (int16_t) time(NULL);
    msg->s_header.s_local_time = timestamp;
    return msg->s_header.s_type;
}

int receive_any(void *self, Message *msg) {
    int8_t id = *(int8_t *) self;
    int type;
    for (int i = 1; i <= count_of_processes; i++) {
        if (i == id) continue;
        if ((type = receive(self, (int8_t) i, msg)) == -1) {
            printf("Error with read from %d to %d\n", id, i);
            return -1; // потом посмотрим
        }
        printf("Receive message %d from %d to %d\n", type, i, id);
    }
    return 0;
}

