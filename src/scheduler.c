#include <stdio.h>
#include <stdlib.h>
#include "scheduler.h"

#define QUANTUM 12

/* FIFO politika */
static pcb_t* fifo_select(process_queue_t* q) {
    return queue_pop(q);
}

/* Ruleta ponderatu aurreratua */
static pcb_t* advanced_roulette_select(process_queue_t* q) {
    int total = 0;
    for (pcb_t* p = q->head; p; p = p->next) {
        int prio = (p->priority == 1) ? 3 : 1;
        total += (1 + p->waiting_time) * prio;
    }

    if (total == 0) return NULL;

    int r = rand() % total;
    pcb_t* prev = NULL;

    for (pcb_t* p = q->head; p; prev = p, p = p->next) {
        int prio = (p->priority == 1) ? 3 : 1;
        r -= (1 + p->waiting_time) * prio;

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
    return (policy == POLICY_RULETA_AVANZATUA)
        ? advanced_roulette_select(q)
        : fifo_select(q);
}

void* scheduler(void* arg) {
    SchedulerParams* params = (SchedulerParams*)arg;
    cpu_system_t* cpu_sys = params->cpu_sys;

    while (!params->shared->done) {

        pthread_mutex_lock(&params->shared->mutex);
        pthread_cond_wait(&params->shared->cond2, &params->shared->mutex);
        pthread_mutex_unlock(&params->shared->mutex);

        printf("\n=== SCHEDULER TICK ===\n");

        /* READY waiting_time eguneratu */
        for (pcb_t* p = params->ready_queue->head; p; p = p->next)
            p->waiting_time++;

        pthread_mutex_lock(&cpu_sys->mutex);

        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {

                    hw_thread_t* hw =
                        &cpu_sys->cpus[c].cores[i].hw_threads[h];

                    if (!hw->current_process) {
                        pcb_t* p = select_next_process(
                            params->ready_queue, params->policy);
                        if (!p) continue;

                        p->state = RUNNING;
                        p->waiting_time = 0;
                        hw->current_process = p;

                        printf(
                          "[DISPATCH] PID=%d -> CPU=%d CORE=%d HW=%d\n",
                          p->pid, c, i, h
                        );
                    }

                    pcb_t* cur = hw->current_process;
                    cur->time_in_cpu++;

                    printf(
                      "[RUNNING] PID=%d | %d/%d\n",
                      cur->pid, cur->time_in_cpu, cur->exec_time
                    );

                    /* Amaiera */
                    if (cur->time_in_cpu >= cur->exec_time) {
                        printf("[TERMINATED] PID=%d\n", cur->pid);
                        cur->state = TERMINATED;
                        queue_push(params->terminated_queue, cur);
                        hw->current_process = NULL;
                    }

                    /* Quantum agortua → READY */
                    else if (cur->time_in_cpu % QUANTUM == 0) {
                        printf("[PREEMPT] PID=%d -> READY\n", cur->pid);
                        cur->state = READY;
                        queue_push(params->ready_queue, cur);
                        hw->current_process = NULL;
                    }
                }
            }
        }

        pthread_mutex_unlock(&cpu_sys->mutex);
    }
    return NULL;
}
