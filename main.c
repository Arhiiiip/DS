#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"
#include "ipc.h"
#include "pa1.h"

const char* log_pipe_open_fmt = "Pipe (%d; %d) created. read_fd=%d, write_fd=%d\n";

typedef struct fd_pair {
    int fd[2];
} fd_pair;

fd_pair** pipes;
local_id X;
local_id p_id = 0;

FILE* events_log_file;
FILE* pipes_log_file;

void task_child(void);
void task_parent(void);

int send(void* self, local_id dst, const Message* msg) {
    long bytes_written = write(((fd_pair*) self)[dst].fd[1], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    return bytes_written != (sizeof(MessageHeader) + msg->s_header.s_payload_len);
}

int send_multicast(void* self, const Message* msg) {
    printf("%s", msg->s_payload);
    fprintf(events_log_file, "%s", msg->s_payload);
    for (local_id i = 0; i <= X; i++) {
        if (i != p_id) {
            if (send(((fd_pair**) self)[p_id], i, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int receive(void* self, local_id from, Message* msg) {
    long bytes_read = read(((fd_pair*) self)[from].fd[0], msg, sizeof(MessageHeader));
    bytes_read += read(((fd_pair*) self)[from].fd[0], &msg->s_payload, msg->s_header.s_payload_len);
    return bytes_read <= 0;
}

int receive_all(void* self, Message* msg) {
    for (local_id i = 1; i <= X; i++) {
        if (i != p_id) {
            memset(msg, 0, MAX_MESSAGE_LEN);
            if (receive(((fd_pair**) self)[i], p_id, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

size_t get_pipes_size(void) {
    return sizeof(struct channel *) * (X + 1);
}

void create_pipes(void) {
    pipes = malloc(get_pipes_size());
    for (local_id i = 0; i <= X; i++) {
        pipes[i] = malloc(get_pipes_size());
        for (local_id j = 0; j <= X; j++) {
            if (i != j) {
                pipe(pipes[i][j].fd);
                fprintf(pipes_log_file, log_pipe_open_fmt, i, j, pipes[i][j].fd[0], pipes[i][j].fd[1]);
            }
        }
    }
}

void close_unused_pipes(void) {
    for (local_id i = 0; i <= X; i++) {
        for (local_id j = 0; j <= X; j++) {
            if (i != p_id && j != p_id && i != j) {
                close(pipes[i][j].fd[0]);
                close(pipes[i][j].fd[1]);
            }
        }
        if (i != p_id) {
            close(pipes[p_id][i].fd[0]);
            close(pipes[i][p_id].fd[1]);
        }
    }
}

void close_pipes(void) {
    for (local_id i = 0; i <= X; i++) {
        if (i != p_id) {
            close(pipes[p_id][i].fd[1]);
            close(pipes[i][p_id].fd[0]);
        }
        free(pipes[i]);
    }
    free(pipes);
}

void init_message_header(Message* msg, MessageType type) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_local_time = 0;
    msg->s_header.s_type = type;
}

void task_child(void) {
    close_unused_pipes();
    Message* msg = malloc(MAX_MESSAGE_LEN);

    init_message_header(msg, STARTED);
    sprintf(msg->s_payload, log_started_fmt, p_id, getpid(), getppid());
    msg->s_header.s_payload_len = strlen(msg->s_payload);
    send_multicast(pipes, msg);

    receive_all(pipes, msg);
    printf(log_received_all_started_fmt, p_id);
    fprintf(events_log_file, log_received_all_started_fmt, p_id);
    memset(msg, 0, MAX_MESSAGE_LEN);

    init_message_header(msg, DONE);
    sprintf(msg->s_payload, log_done_fmt, p_id);
    msg->s_header.s_payload_len = strlen(msg->s_payload);
    send_multicast(pipes, msg);

    receive_all(pipes, msg);
    printf(log_received_all_done_fmt, p_id);
    fprintf(events_log_file, log_received_all_done_fmt, p_id);

    free(msg);
    close_pipes();
}

void task_parent(void) {
    close_unused_pipes();
    Message* msg = malloc(MAX_MESSAGE_LEN);

    receive_all(pipes, msg);
    receive_all(pipes, msg);
    while (wait(NULL) > 0);

    free(msg);
    close_pipes();
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        X = (local_id) atoi(argv[2]);
        if (X > 0 && X < 11) {
            events_log_file = fopen(events_log, "w");
            pipes_log_file = fopen(pipes_log, "w");

            create_pipes();

            for (local_id i = 1; i <= X; i++) {
                if (fork() == 0) {
                    p_id = i;
                    task_child();
                    return 0;
                }
            }
            task_parent();

            fclose(pipes_log_file);
            fclose(events_log_file);
        } else {
            printf("Value must be in range [1..10]!");
            return 1;
        }
    } else {
        printf("Usage: -p <value>");
        return 1;
    }
    return 0;
}
