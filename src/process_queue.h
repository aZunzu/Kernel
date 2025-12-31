#ifndef PROCESS_QUEUE_H
#define PROCESS_QUEUE_H

#include <pthread.h>
#include "pcb.h"

typedef struct {
    pcb_t* head;
    pcb_t* tail;
    pthread_mutex_t mutex;
} process_queue_t;

void queue_init(process_queue_t* q);
void queue_push(process_queue_t* q, pcb_t* pcb);
pcb_t* queue_pop(process_queue_t* q);
int queue_is_empty(process_queue_t* q);
int queue_count(process_queue_t* q);
#endif
