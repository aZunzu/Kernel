#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "process_generator.h"
#include "pcb.h"

static int next_pid = 1;

void* process_generator(void* arg) {
    ProcessGenParams* params = (ProcessGenParams*)arg;

    while (!params->shared->done) {
        pthread_mutex_lock(&params->shared->mutex);
        pthread_cond_wait(&params->shared->cond, &params->shared->mutex);
        pthread_mutex_unlock(&params->shared->mutex);

        int priority = rand() % 2; // normal / urgente
        pcb_t* p = pcb_create(next_pid++, priority);
        p->state = READY;

        queue_push(params->ready_queue, p);

        printf("[Process Generator] PID %d creado (prio=%d)\n",
               p->pid, p->priority);
    }
    return NULL;
}
