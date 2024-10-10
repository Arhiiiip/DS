#ifndef PA1_STARTER_CODE_THREAD_H
#define PA1_STARTER_CODE_THREAD_H

#include "ipc.h"

#define READ 0
#define WRITE 1

enum event_type {
    PROC_STARTED = 0,
    PROC_RECEIVE_ALL_STARTED = 1,
    PROC_DONE = 2,
    PROC_RECEIVE_ALL_DONE = 3
};

typedef struct {
    int fd[2]; // in - 0, out - 1
} u_pipe;

int count_of_processes;
u_pipe pipes[MAX_PROCESS_ID][MAX_PROCESS_ID];

#endif //PA1_STARTER_CODE_THREAD_H
