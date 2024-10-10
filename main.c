#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdarg.h>  // Добавьте этот заголовочный файл
#include <stdbool.h>
#include "common.h"
#include "ipc.h"
#include "pa1.h"
#include "thread.h"

local_id X;
local_id cur_id = 0;
FILE* events_log_file;
FILE* pipes_log_file;

typedef struct {
    int fd[2];
} Pipe;

Pipe** pipes;

void create_log_files() {
    events_log_file = fopen(events_log, "a");
    if (events_log_file == NULL) {
        printf("Can't open file \"%s\"!\n", events_log);
        exit(-1);
    }
    pipes_log_file = fopen(pipes_log, "a");
    if (pipes_log_file == NULL) {
        printf("Can't open file \"%s\"!\n", pipes_log);
        exit(-1);
    }
}

void create_pipes() {
    pipes = malloc(sizeof(Pipe*) * (X + 1));
    for (local_id i = 0; i <= X; i++) {
        pipes[i] = malloc(sizeof(Pipe) * (X + 1));
        for (local_id j = 0; j <= X; j++) {
            if (i != j) {
                if (pipe(pipes[i][j].fd) < 0) {
                    printf("Can't create pipe\n");
                    exit(1);
                }
                fprintf(pipes_log_file, "Pipe from %d to %d created, R: %d  W: %d\n",
                        i, j, pipes[i][j].fd[READ], pipes[i][j].fd[WRITE]);
            }
        }
    }
}

void close_unused_pipes() {
    for (local_id i = 0; i <= X; i++) {
        for (local_id j = 0; j <= X; j++) {
            if (i != cur_id && j != cur_id && i != j) {
                close(pipes[i][j].fd[READ]);
                close(pipes[i][j].fd[WRITE]);
            }
        }
        if (i != cur_id) {
            close(pipes[cur_id][i].fd[READ]);
            close(pipes[i][cur_id].fd[WRITE]);
        }
    }
}

void close_all_pipes() {
    for (local_id i = 0; i <= X; i++) {
        if (i != cur_id) {
            close(pipes[cur_id][i].fd[WRITE]);
            close(pipes[i][cur_id].fd[READ]);
        }
        free(pipes[i]);
    }
    free(pipes);
}

void send_message(local_id dst, const Message* msg) {
    write(pipes[cur_id][dst].fd[WRITE], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
}

void send_multicast(const Message* msg) {
    for (local_id i = 0; i <= X; i++) {
        if (i != cur_id) {
            send_message(i, msg);
        }
    }
}

void receive_message(local_id from, Message* msg) {
    read(pipes[from][cur_id].fd[READ], msg, sizeof(MessageHeader));
    read(pipes[from][cur_id].fd[READ], msg->s_payload, msg->s_header.s_payload_len);
}

void receive_all(Message* msg) {
    for (local_id i = 0; i <= X; i++) {
        if (i != cur_id) {
            receive_message(i, msg);
        }
    }
}

void log_event(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    vfprintf(events_log_file, fmt, args);
    va_end(args);
}

void task(bool isChild) {
    close_unused_pipes();
    Message msg;

    if (isChild) {
        msg.s_header.s_magic = MESSAGE_MAGIC;
        msg.s_header.s_type = STARTED;
        sprintf(msg.s_payload, log_started_fmt, cur_id, getpid(), getppid());
        msg.s_header.s_payload_len = strlen(msg.s_payload);
        send_multicast(&msg);
        log_event(log_started_fmt, cur_id, getpid(), getppid());

        receive_all(&msg);
        log_event(log_received_all_started_fmt, cur_id);

        msg.s_header.s_type = DONE;
        sprintf(msg.s_payload, log_done_fmt, cur_id);
        msg.s_header.s_payload_len = strlen(msg.s_payload);
        send_multicast(&msg);
        log_event(log_done_fmt, cur_id);

        receive_all(&msg);
        log_event(log_received_all_done_fmt, cur_id);
    } else {
        receive_all(&msg);
        receive_all(&msg);
        while (wait(NULL) > 0);
    }

    close_all_pipes();
    fclose(events_log_file);
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        X = (local_id) atoi(argv[2]);
        if (X > 0 && X < 11) {
            create_log_files();
            create_pipes();
            fclose(pipes_log_file);

            for (local_id i = 1; i <= X; i++) {
                if (fork() == 0) {
                    cur_id = i;
                    task(true);
                    return 0;
                }
            }
            task(false);
        } else {
            printf("Value must be in range [1..10]!\n");
            return 1;
        }
    } else {
        printf("Usage: -p <value>\n");
        return 1;
    }
    return 0;
}
