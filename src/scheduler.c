#include <stdio.h>
#include <stdlib.h>
#include "scheduler.h"

#define MIN_EXEC_TIME 2
#define MAX_EXEC_TIME 6

/* FIFO politika */
static pcb_t* fifo_select(process_queue_t* q) {
    return queue_pop(q);
}

/* Ruleta ponderatu aurreratua */
static pcb_t* advanced_roulette_select(process_queue_t* q) {
    int total_weight = 0;
    pcb_t* p;

    /* Pisu osoa kalkulatu */
    for (p = q->head; p != NULL; p = p->next) {
        int priority_factor = (p->priority == 1) ? 3 : 1;
        total_weight += (1 + p->waiting_time) * priority_factor;
    }

    if (total_weight == 0) return NULL;

    int r = rand() % total_weight;
    pcb_t* prev = NULL;

    /* Prozesua aukeratu */
    for (p = q->head; p != NULL; prev = p, p = p->next) {
        int priority_factor = (p->priority == 1) ? 3 : 1;
        r -= (1 + p->waiting_time) * priority_factor;

        if (r < 0) {
            if (prev) prev->next = p->next;
            else q->head = p->next;

            if (p == q->tail) q->tail = prev;
            p->next = NULL;
            return p;
        }
    }
    return NULL;
}

/* Politika nagusia */
pcb_t* select_next_process(process_queue_t* q, sched_policy_t policy) {
    switch (policy) {
        case POLICY_RULETA_AVANZATUA:
            return advanced_roulette_select(q);
        case POLICY_FIFO:
        default:
            return fifo_select(q);
    }
}

void* scheduler(void* arg) {
    SchedulerParams* params = (SchedulerParams*)arg;
    cpu_system_t* cpu_sys = params->cpu_sys;

    while (!params->shared->done) {

        pthread_mutex_lock(&params->shared->mutex);
        pthread_cond_wait(&params->shared->cond2, &params->shared->mutex);
        pthread_mutex_unlock(&params->shared->mutex);

        /* READY prozesuen waiting_time eguneratu */
        pcb_t* wp;
        for (wp = params->ready_queue->head; wp != NULL; wp = wp->next)
            wp->waiting_time++;

        pthread_mutex_lock(&cpu_sys->mutex);

        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {

                    hw_thread_t* hw =
                        &cpu_sys->cpus[c].cores[i].hw_threads[h];

                    if (!hw->current_process) {
                        pcb_t* p = select_next_process(
                            params->ready_queue,
                            params->policy
                        );
                        if (!p) continue;

                        p->state = RUNNING;
                        p->waiting_time = 0;
                        hw->current_process = p;

                        printf("[Scheduler] PID %d HW thread %d-n\n",
                               p->pid, h);
                    }

                    pcb_t* cur = hw->current_process;
                    cur->time_in_cpu++;

                    /* Amaiera edo segurtasun muga */
                    if (cur->time_in_cpu >= cur->exec_time ||
                        cur->time_in_cpu >= MAX_EXEC_TIME) {

                        printf("[Scheduler] PID %d TERMINATED\n", cur->pid);
                        cur->state = TERMINATED;
                        queue_push(params->terminated_queue, cur);
                        hw->current_process = NULL;
                    }
                }
            }
        }

        pthread_mutex_unlock(&cpu_sys->mutex);
    }
    return NULL;
}
