#include <stdio.h>
#include <stdlib.h>
#include "process_generator.h"
#include "pcb.h"

void* process_generator(void* arg) {
    ProcessGenParams* params = (ProcessGenParams*)arg;

    while (params->shared->sim_running) {

        pthread_mutex_lock(&params->shared->mutex);
        pthread_cond_wait(&params->shared->cond, &params->shared->mutex);
        // Baliteke seinalea generadoreak 'lapurtzea'; berresalbu clock-erako
        pthread_cond_signal(&params->shared->cond);
        pthread_mutex_unlock(&params->shared->mutex);

        // Baldin eta probabilitatea konfiguratu bada, erabili hori
        // Bestela, beti sortu (100%)
        int prob = (params->probability > 0) ? params->probability : 100;
        
        if (rand() % 100 < prob) {
            // PID berria hartu kontagailu partekatutik
            int pid = 0;
            pthread_mutex_lock(&params->shared->mutex);
            pid = (*params->next_pid)++;
            pthread_mutex_unlock(&params->shared->mutex);

            pcb_t* p = pcb_create(pid, rand() % 2);
            p->state = READY;
            p->waiting_time = 0;
            p->exec_time = 2 + rand() % 6;  // Exekuzio denbora aleatorioa (2-7)

            queue_push(params->ready_queue, p);

            printf("[PROCESS GENERATOR] PID=%d sortuta (Exec=%d, Prio=%d)\n", 
                   p->pid, p->exec_time, p->priority);
        }
    }
    return NULL;
}
