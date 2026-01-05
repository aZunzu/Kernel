#include <stdlib.h>
#include "process_queue.h"

// Prozesu ilara hasieratu
void queue_init(process_queue_t* q) {
    q->head = q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
}

// Prozesu bat ilarara gehitu
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

// Prozesu bat ilaratik atera
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

// Egiaztatu ilara hutsik dagoen
int queue_is_empty(process_queue_t* q) {
    pthread_mutex_lock(&q->mutex);
    int empty = (q->head == NULL);
    pthread_mutex_unlock(&q->mutex);
    return empty;
}

// Prozesu kopurua lortu
int queue_count(process_queue_t* q) {
    pthread_mutex_lock(&q->mutex);
    
    int count = 0;
    pcb_t* current = q->head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    
    pthread_mutex_unlock(&q->mutex);
    return count;
}