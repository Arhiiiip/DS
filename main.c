#define READ 0
#define WRITE 1

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

typedef struct pipe {
    int fd[2];
} pipe;

pipe** pipes;
local_id X;
local_id p_id = 0;

FILE* events_log_file;
FILE* pipes_log_file;

void task_isChild(void);
void task_isParent(void);

int send(void* self, local_id dst, const Message* msg) {
    long bytes_written = write(((pipe*) self)[dst].fd[WRITE], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    return bytes_written != (sizeof(MessageHeader) + msg->s_header.s_payload_len);
}

int send_multicast(void* self, const Message* msg) {
    printf("%s", msg->s_payload);
    fprintf(events_log_file, "%s", msg->s_payload);
    for (local_id i = 0; i <= X; i++) {
        if (i != p_id) {
            if (send(((pipe**) self)[p_id], i, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int receive(void* self, local_id from, Message* msg) {
    long bytes_read = read(((pipe*) self)[from].fd[READ], msg, sizeof(MessageHeader));
    bytes_read += read(((pipe*) self)[from].fd[READ], &msg->s_payload, msg->s_header.s_payload_len);
    return bytes_read <= 0;
}

int receive_all(void* self, Message* msg) {
    for (local_id i = 1; i <= X; i++) {
        if (i != p_id) {
            memset(msg, 0, MAX_MESSAGE_LEN);
            if (receive(((pipe**) self)[i], p_id, msg) != 0) {
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
                fprintf(pipes_log_file, log_pipe_open_fmt, i, j, pipes[i][j].fd[READ], pipes[i][j].fd[WRITE]);
            }
        }
    }
}

void close_unused_pipes(void) {
    for (local_id i = 0; i <= X; i++) {
        for (local_id j = 0; j <= X; j++) {
            if (i != p_id && j != p_id && i != j) {
                close(pipes[i][j].fd[READ]);
                close(pipes[i][j].fd[WRITE]);
            }
        }
        if (i != p_id) {
            close(pipes[p_id][i].fd[READ]);
            close(pipes[i][p_id].fd[WRITE]);
        }
    }
}

void close_pipes(void) {
    for (local_id i = 0; i <= X; i++) {
        if (i != p_id) {
            close(pipes[p_id][i].fd[WRITE]);
            close(pipes[i][p_id].fd[READ]);
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

void spread(Message* msg, MessageType type) {
    init_message_header(msg, type);
    if(type == STARTED) {
        sprintf(msg->s_payload, log_started_fmt, p_id, getpid(), getppid());
    }
    if(type == DONE) {
        sprintf(msg->s_payload, log_done_fmt, p_id);
    }
    msg->s_header.s_payload_len = strlen(msg->s_payload);
    send_multicast(pipes, msg);
}

void task_isChild(void) {
    close_unused_pipes();
    Message* msg = malloc(MAX_MESSAGE_LEN);
    spread(msg, STARTED);
    receive_all(pipes, msg);
    printf(log_received_all_started_fmt, p_id);
    fprintf(events_log_file, log_received_all_started_fmt, p_id);
    memset(msg, 0, MAX_MESSAGE_LEN);
    spread(msg, DONE);
    receive_all(pipes, msg);
    printf(log_received_all_done_fmt, p_id);
    fprintf(events_log_file, log_received_all_done_fmt, p_id);

    free(msg);
    close_pipes();
}

void task_isParent(void) {
    close_unused_pipes();
    Message* msg = malloc(MAX_MESSAGE_LEN);

    receive_all(pipes, msg);
    receive_all(pipes, msg);
    while (wait(NULL) > 0);

    free(msg);
    close_pipes();
}

int main(int argc, char** argv) {
    events_log_file = fopen(events_log, "w");
    pipes_log_file = fopen(pipes_log, "w");
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        X = (local_id) atoi(argv[2]);
        if (X > 0 && X < 11) {
            create_pipes();

            for (local_id i = 1; i <= X; i++) {
                if (fork() == 0) {
                    p_id = i;
                    task_isChild();
                    return 0;
                }
            }
            task_isParent();
        } else {
            printf("Value must be in range [1..10]!");
            return 1;
        }
    } else {
        printf("Usage: -p <value>");
        return 1;
    }
    fclose(pipes_log_file);
    fclose(events_log_file);
    return 0;
}
