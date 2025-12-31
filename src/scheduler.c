#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "scheduler.h"
#include "process_queue.h"
#include "cpu.h"
#include "pcb.h"

#define QUANTUM 3  // Laburragoa simulazio errealago baterako

/* =======================================================
 * FUNTZIO LAGUNTZAILEAK
 * ======================================================= */

// FIFO politika
static pcb_t* fifo_select(process_queue_t* q) {
    return queue_pop(q);
}

// Ruleta ponderatu aurreratua
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

// Hurrengo prozesua hautatzeko funtzio nagusia
pcb_t* select_next_process(process_queue_t* q, sched_policy_t policy) {
    return (policy == POLICY_RULETA_AVANZATUA)
        ? advanced_roulette_select(q)
        : fifo_select(q);
}

/* =======================================================
 * SCHEDULER NAGUSIA
 * ======================================================= */

void* scheduler(void* arg) {
    SchedulerParams* params = (SchedulerParams*)arg;
    cpu_system_t* cpu_sys = params->cpu_sys;
    
    // Ausazko zenbakiak hasieratu
    srand(time(NULL));
    
    printf("[SCHEDULER] Hasieratuta - Politika: %s\n", 
           (params->policy == POLICY_RULETA_AVANZATUA) ? "Ruleta Aurreratua" : "FIFO");
    
    // Scheduler-aren begizta nagusia
    while (params->shared->sim_running) {
        pthread_mutex_lock(&params->shared->mutex);
        
        // Simulazioa gelditu bada, irten
        if (!params->shared->sim_running) {
            pthread_mutex_unlock(&params->shared->mutex);
            break;
        }
        
        // Tick bat exekutatzeko itxaron
        pthread_cond_wait(&params->shared->cond2, &params->shared->mutex);
        pthread_mutex_unlock(&params->shared->mutex);
        
        // Simulazioa gelditu bada, irten
        if (!params->shared->sim_running) {
            break;
        }
        
        // ===== 1. READY PROZESUEN WAITING_TIME EGUNERATU =====
        for (pcb_t* p = params->ready_queue->head; p; p = p->next) {
            if (p->state == READY) {
                p->waiting_time++;
            }
        }
        
        pthread_mutex_lock(&cpu_sys->mutex);
        
        // ===== 2. EXEKUTATZE FASE =====
        int executed_this_tick = 0;
        int completed_this_tick = 0;
        
        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                    
                    hw_thread_t* hw = &cpu_sys->cpus[c].cores[i].hw_threads[h];
                    pcb_t* cur = hw->current_process;
                    
                    if (cur && cur->state == RUNNING) {
                        // Prozesua exekutatu
                        cur->time_in_cpu++;
                        executed_this_tick++;
                        
                        // Erakutsi exekuzioaren aurrerapena
                        if (cur->time_in_cpu == 1) {
                            printf("  🚀 PID=%d exekutatzen hasi da (HW %d-%d-%d)\n", 
                                   cur->pid, c, i, h);
                        }
                        
                        // Exekuzioa bukatu bada
                        if (cur->time_in_cpu >= cur->exec_time) {
                            printf("  ✅ PID=%d bukatu da (%d/%d tick)\n", 
                                   cur->pid, cur->time_in_cpu, cur->exec_time);
                            cur->state = TERMINATED;
                            queue_push(params->terminated_queue, cur);
                            hw->current_process = NULL;
                            completed_this_tick++;
                            continue;
                        }
                        
                        // Quantum bukatu bada
                        if (cur->time_in_cpu % QUANTUM == 0) {
                            printf("  🔄 PID=%d → READY (quantum %d/%d)\n", 
                                   cur->pid, cur->time_in_cpu / QUANTUM, 
                                   (cur->exec_time + QUANTUM - 1) / QUANTUM);
                            cur->state = READY;
                            queue_push(params->ready_queue, cur);
                            hw->current_process = NULL;
                        } else if (cur->time_in_cpu % 2 == 0) {
                            // Exekuzioaren aurrerapena erakutsi (2 tickero)
                            printf("  📊 PID=%d exekutatzen: %d/%d (%d%%)\n",
                                   cur->pid, cur->time_in_cpu, cur->exec_time,
                                   (cur->time_in_cpu * 100) / cur->exec_time);
                        }
                    }
                }
            }
        }
        
        // Exekutatu gabe badaude HW thread libreak, erakutsi
        if (executed_this_tick == 0) {
            printf("  💤 Ez dago exekutatzen ari den prozesurik\n");
        } else {
            printf("  ⚡ %d prozesu exekutatu (%d bukatu)\n", 
                   executed_this_tick, completed_this_tick);
        }
        
        // ===== 3. ESLEIPEN FASE =====
        int dispatched_this_tick = 0;
        
        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                    
                    hw_thread_t* hw = &cpu_sys->cpus[c].cores[i].hw_threads[h];
                    
                    if (!hw->current_process) {
                        // Prozesu bat hautatu
                        pcb_t* p = select_next_process(params->ready_queue, params->policy);
                        if (!p) {
                            continue;  // Ez dago prozesurik
                        }
                        
                        // Prozesua esleitu
                        p->state = RUNNING;
                        p->waiting_time = 0;
                        hw->current_process = p;
                        dispatched_this_tick++;
                        
                        printf("  📌 PID=%d → HW %d-%d-%d (Prio=%d)\n", 
                               p->pid, c, i, h, p->priority);
                    }
                }
            }
        }
        
        if (dispatched_this_tick == 0) {
            // Egiaztatu READY ilaran prozesurik dagoen
            int ready_count = 0;
            for (pcb_t* p = params->ready_queue->head; p; p = p->next) {
                if (p->state == READY) ready_count++;
            }
            
            if (ready_count > 0) {
                printf("  ⚠️  READY ilaran %d prozesu, baina HW guztiak okupatuta\n", 
                       ready_count);
            }
        } else {
            printf("  🎯 %d prozesu esleitu\n", dispatched_this_tick);
        }
        
        // ===== 4. BLOCKED PROZESUAK KUDEATU (I/O) =====
        // Simulazioa: zenbait prozesu ausaz READY-ra itzuli
        pcb_t* prev = NULL;
        pcb_t* current = params->blocked_queue->head;
        
        while (current) {
            // %20 probabilitatea I/O amaitzeko
            if (rand() % 100 < 20) {
                pcb_t* to_unblock = current;
                
                // Kendu blocked ilaratik
                if (prev) {
                    prev->next = current->next;
                } else {
                    params->blocked_queue->head = current->next;
                }
                
                if (current == params->blocked_queue->tail) {
                    params->blocked_queue->tail = prev;
                }
                
                current = current->next;
                
                // Gehitu ready ilarara
                to_unblock->state = READY;
                to_unblock->next = NULL;
                queue_push(params->ready_queue, to_unblock);
                
                printf("  🔓 PID=%d I/O amaitu → READY\n", to_unblock->pid);
            } else {
                prev = current;
                current = current->next;
            }
        }
        
        // ===== 5. ESTATISTIKAK EGUNERATU =====
        if (params->shared->sim_tick % 5 == 0) {
            printf("\n  📊 TICK %d - SISTEMA ESTATISTIKAK:\n", params->shared->sim_tick);
            
            // Kontatu prozesu mota bakoitzeko
            int running_count = 0;
            int ready_count = 0;
            int blocked_count = 0;
            int terminated_count = 0;
            
            for (int c = 0; c < cpu_sys->cpu_kop; c++) {
                for (int i = 0; i < cpu_sys->core_kop; i++) {
                    for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                        if (cpu_sys->cpus[c].cores[i].hw_threads[h].current_process) {
                            running_count++;
                        }
                    }
                }
            }
            
            for (pcb_t* p = params->ready_queue->head; p; p = p->next) {
                if (p->state == READY) ready_count++;
            }
            
            for (pcb_t* p = params->blocked_queue->head; p; p = p->next) {
                if (p->state == BLOCKED) blocked_count++;
            }
            
            for (pcb_t* p = params->terminated_queue->head; p; p = p->next) {
                if (p->state == TERMINATED) terminated_count++;
            }
            
            printf("    • RUNNING: %d prozesu\n", running_count);
            printf("    • READY: %d prozesu\n", ready_count);
            printf("    • BLOCKED: %d prozesu\n", blocked_count);
            printf("    • TERMINATED: %d prozesu\n", terminated_count);
            printf("    • CPU erabilera: %d/%d HW thread (%.0f%%)\n",
                   running_count,
                   cpu_sys->cpu_kop * cpu_sys->core_kop * cpu_sys->hw_thread_kop,
                   (running_count * 100.0) / 
                   (cpu_sys->cpu_kop * cpu_sys->core_kop * cpu_sys->hw_thread_kop));
        }
        
        pthread_mutex_unlock(&cpu_sys->mutex);
        
        // Labur bat itxaron simulazioa errealagoa izateko
        usleep(100000);  // 0.1 segundo
    }
    
    printf("[SCHEDULER] Amaituta\n");
    return NULL;
}

/* =======================================================
 * PROZESUEN ANALISIA (estatistikak kalkulatzeko)
 * ======================================================= */

void analyze_processes(SchedulerParams* params) {
    int total_waiting = 0;
    int total_turnaround = 0;
    int process_count = 0;
    
    printf("\n=== PROZESUEN ANALISIA ===\n");
    
    // Terminated prozesuak analizatu
    pcb_t* p = params->terminated_queue->head;
    while (p) {
        if (p->state == TERMINATED) {
            // Simulaketa: turnaround_time = exec_time + waiting_time
            int turnaround_time = p->exec_time + p->waiting_time;
            total_waiting += p->waiting_time;
            total_turnaround += turnaround_time;
            process_count++;
            
            printf("PID=%d: Exec=%d, Wait=%d, Turnaround=%d\n",
                   p->pid, p->exec_time, p->waiting_time, turnaround_time);
        }
        p = p->next;
    }
    
    if (process_count > 0) {
        printf("\nBATEZ BESTEKOAK:\n");
        printf("• Waiting Time: %.2f tick\n", (float)total_waiting / process_count);
        printf("• Turnaround Time: %.2f tick\n", (float)total_turnaround / process_count);
    }
}