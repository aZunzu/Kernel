#include <stdlib.h>
#include "process_queue.h"

void queue_init(process_queue_t* q) {
    q->head = q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
}

void queue_push(process_queue_t* q, pcb_t* pcb) {
    pthread_mutex_lock(&q->mutex);

    pcb->next = NULL;
    if (q->tail) {
        q->tail->next = pcb;
    } else {
        q->head = pcb;
    }
    q->tail = pcb;

    pthread_mutex_unlock(&q->mutex);
}

pcb_t* queue_pop(process_queue_t* q) {
    pthread_mutex_lock(&q->mutex);

    pcb_t* p = q->head;
    if (p) {
        q->head = p->next;
        if (!q->head) q->tail = NULL;
    }

    pthread_mutex_unlock(&q->mutex);
    return p;
}

int queue_is_empty(process_queue_t* q) {
    pthread_mutex_lock(&q->mutex);
    int empty = (q->head == NULL);
    pthread_mutex_unlock(&q->mutex);
    return empty;
}
