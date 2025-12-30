#include <stdio.h>
#include <stdlib.h>
#include "scheduler.h"

#define QUANTUM 25

/* FIFO politika */
static pcb_t* fifo_select(process_queue_t* q) {
    return queue_pop(q);
}

/* Ruleta ponderatu aurreratua */
static pcb_t* advanced_roulette_select(process_queue_t* q) {
    int total = 0;
    for (pcb_t* p = q->head; p; p = p->next) {
        if (p->state == READY) {
            int prio = (p->priority == 1) ? 3 : 1;  // Premiazkoa: x3
            total += (1 + p->waiting_time) * prio;
        }
    }

    if (total == 0) return NULL;

    int r = rand() % total;
    pcb_t* prev = NULL;

    for (pcb_t* p = q->head; p; prev = p, p = p->next) {
        if (p->state == READY) {
            int prio = (p->priority == 1) ? 3 : 1;
            r -= (1 + p->waiting_time) * prio;

            if (r < 0) {
                // Prozesua ilaratik kendu
                if (prev) {
                    prev->next = p->next;
                } else {
                    q->head = p->next;
                }
                if (p == q->tail) q->tail = prev;
                p->next = NULL;
                return p;
            }
        }
    }
    return NULL;
}

/* Hurrengo prozesua hautatzeko funtzio nagusia */
pcb_t* select_next_process(process_queue_t* q, sched_policy_t policy) {
    return (policy == POLICY_RULETA_AVANZATUA)
        ? advanced_roulette_select(q)
        : fifo_select(q);
}

/* TICK globalen kontadorea (sistema osoarentzat) */
static int global_tick = 0;

/* SCHEDULER nagusia */
void* scheduler(void* arg) {
    SchedulerParams* params = (SchedulerParams*)arg;
    cpu_system_t* cpu_sys = params->cpu_sys;

    while (!params->shared->done) {
        pthread_mutex_lock(&params->shared->mutex);
        pthread_cond_wait(&params->shared->cond2, &params->shared->mutex);
        pthread_mutex_unlock(&params->shared->mutex);

        global_tick++;
        printf("\n=== SCHEDULER TICK #%d ===\n", global_tick);
        
        // ===== KONFIGURAZIOA =====
        printf("[KONFIG] %d CPU × %d CORE × %d HW = %d hilo total\n",
               cpu_sys->cpu_kop, cpu_sys->core_kop, cpu_sys->hw_thread_kop,
               cpu_sys->cpu_kop * cpu_sys->core_kop * cpu_sys->hw_thread_kop);

        // ===== 1. READY prozesuen waiting_time eguneratu =====
        int ready_count = 0;
        for (pcb_t* p = params->ready_queue->head; p; p = p->next) {
            if (p->state == READY) {
                p->waiting_time++;
                ready_count++;
            }
        }
        printf("[READY] %d prozesu itxaroten\n", ready_count);

        pthread_mutex_lock(&cpu_sys->mutex);

        // ===== 2. EXEKUTATZE FASE =====
        printf("\n[EXEKUTATZE FASE]\n");
        int executed_this_tick = 0;
        
        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {

                    hw_thread_t* hw = &cpu_sys->cpus[c].cores[i].hw_threads[h];
                    pcb_t* cur = hw->current_process;

                    if (cur && cur->state == RUNNING) {
                        cur->time_in_cpu++;
                        executed_this_tick++;
                        
                        printf("  PID=%d | exec=%d/%d | HW=%d-%d-%d\n",
                               cur->pid, cur->time_in_cpu, cur->exec_time,
                               c, i, h);
                        
                        if (cur->time_in_cpu >= cur->exec_time) {
                            printf("[TERMINATED] PID=%d bukatu da\n", cur->pid);
                            cur->state = TERMINATED;
                            queue_push(params->terminated_queue, cur);
                            hw->current_process = NULL;
                            continue;
                        }
                        
                        if (cur->time_in_cpu % QUANTUM == 0) {
                            printf("[PREEMPT] PID=%d → READY (quantum)\n", cur->pid);
                            cur->state = READY;
                            queue_push(params->ready_queue, cur);
                            hw->current_process = NULL;
                        }
                    }
                }
            }
        }
        
        if (executed_this_tick == 0) {
            printf("  (ez dago exekutatzen ari den prozesurik)\n");
        } else {
            printf("[EXEK] %d prozesu exekutatu\n", executed_this_tick);
        }

        // ===== 3. ESLEIPEN FASE =====
        printf("\n[ESLEIPEN FASE]\n");
        int dispatched_this_tick = 0;
        
        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {

                    hw_thread_t* hw = &cpu_sys->cpus[c].cores[i].hw_threads[h];
                    
                    if (!hw->current_process) {
                        pcb_t* p = select_next_process(params->ready_queue, params->policy);
                        if (!p) {
                            printf("  HW %d-%d-%d: LIBRE\n", c, i, h);
                            continue;
                        }
                        
                        p->state = RUNNING;
                        p->waiting_time = 0;
                        hw->current_process = p;
                        dispatched_this_tick++;
                        
                        printf("  PID=%d → HW %d-%d-%d\n", 
                               p->pid, c, i, h);
                    }
                }
            }
        }
        
        if (dispatched_this_tick == 0) {
            printf("  (ez da prozesurik esleitu)\n");
        } else {
            printf("[ESLEIPEN] %d prozesu esleitu\n", dispatched_this_tick);
        }
        
        // ===== 4. ESTADISTIKAK (EGIAK) =====
        printf("\n[EGUNGOREN ESTATUAK]\n");
        int running_count = 0;
        int idle_hw = 0;
        
        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                    if (cpu_sys->cpus[c].cores[i].hw_threads[h].current_process) {
                        running_count++;
                    } else {
                        idle_hw++;
                    }
                }
            }
        }
        
        printf("RUNNING prozesuak: %d\n", running_count);
        printf("HW Thread okupatuak: %d\n", running_count);
        printf("HW Thread libreak: %d\n", idle_hw);
        printf("GUZTIRA: %d\n", running_count + idle_hw);
        printf("Ticks sistema: %d\n", global_tick);

        pthread_mutex_unlock(&cpu_sys->mutex);
        
        printf("=== TICK #%d BUKATUTA ===\n", global_tick);
    }
    
    printf("[SCHEDULER] %d tick-etan bukatu da\n", global_tick);
    return NULL;
}