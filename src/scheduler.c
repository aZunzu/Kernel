#include <stdio.h>
#include <stdlib.h>
#include "scheduler.h"
#include "cpu.h"

#define MIN_EXEC_TIME 2
#define MAX_EXEC_TIME 6

void* scheduler(void* arg) {
    SchedulerParams* params = (SchedulerParams*)arg;
    cpu_system_t* cpu_sys = params->cpu_sys;

    while (!params->shared->done) {

        pthread_mutex_lock(&params->shared->mutex);
        pthread_cond_wait(&params->shared->cond2, &params->shared->mutex);
        pthread_mutex_unlock(&params->shared->mutex);

        pthread_mutex_lock(&cpu_sys->mutex);

        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {

                    hw_thread_t* hw =
                        &cpu_sys->cpus[c].cores[i].hw_threads[h];

                    if (!hw->current_process) {
                        pcb_t* p = queue_pop(params->ready_queue);
                        if (!p) continue;

                        p->state = RUNNING;
                        hw->current_process = p;

                        printf("[Scheduler] PID %d HW thread %d-n\n",
                               p->pid, h);
                    }

                    pcb_t* cur = hw->current_process;
                    cur->time_in_cpu++;

                    if (cur->time_in_cpu >= cur->exec_time ||
                        cur->time_in_cpu >= MAX_EXEC_TIME) {

                        printf("[Scheduler] PID %d amaituta\n", cur->pid);
                        cur->state = TERMINATED;
                        free(cur);
                        hw->current_process = NULL;
                    }
                }
            }
        }

        pthread_mutex_unlock(&cpu_sys->mutex);
    }
    return NULL;
}
