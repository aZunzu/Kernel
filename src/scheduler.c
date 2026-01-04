#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "scheduler.h"
#include "process_queue.h"
#include "cpu.h"
#include "pcb.h"
#include "execution.h"
#include "loader.h"

#define SAFETY_QUANTUM 50  // Safety net: 50 tick baino gehiago exekutatzen badu

/* =======================================================
 * FUNTZIO LAGUNTZAILEAK
 * ======================================================= */

// Funtzio laguntzaileak prozesu motaren izena lortzeko
static const char* get_process_type_name(process_type_t type) {
    switch(type) {
        case PROCESS_TICK_BASED: return "TICK bidezkoa";
        case PROCESS_INSTRUCTION_BASED: return "INSTRUKZIO bidezkoa";
        default: return "EZEZAGUNA";
    }
}

static const char* get_process_type_short(process_type_t type) {
    switch(type) {
        case PROCESS_TICK_BASED: return "TICK";
        case PROCESS_INSTRUCTION_BASED: return "INSTR";
        default: return "???";
    }
}

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
 * SCHEDULER NAGUSIA - HIBRIDOA (TICK + INSTRUKZIO BIDEZKOA)
 * ======================================================= */

void* scheduler(void* arg) {
    SchedulerParams* params = (SchedulerParams*)arg;
    cpu_system_t* cpu_sys = params->cpu_sys;
    
    // Ausazko zenbakiak hasieratu
    srand(time(NULL));
    
    printf("[SCHEDULER] Hasieratuta - Sistema hibridoa\n");
    printf("[SCHEDULER] Prozesu motak: TICK bidezkoak eta INSTRUKZIO bidezkoak\n");
    printf("[SCHEDULER] Timer-ak aktibatuko du exekuzio bakoitzean\n");
    
    int scheduler_tick_count = 0;
    int safety_preemptions = 0;
    int total_instructions_executed = 0;
    
    while (params->shared->sim_running) {
        pthread_mutex_lock(&params->shared->mutex);
        
        // Timer-aren seinalea itxaron
        while (params->shared->scheduler_signal == 0 && params->shared->sim_running) {
            pthread_cond_wait(&params->shared->cond_scheduler, &params->shared->mutex);
        }
        
        // Seinalea berrezarri hurrengorako
        params->shared->scheduler_signal = 0;
        
        // Simulazioa gelditu bada, irten
        if (!params->shared->sim_running) {
            pthread_mutex_unlock(&params->shared->mutex);
            break;
        }
        
        pthread_mutex_unlock(&params->shared->mutex);
        
        scheduler_tick_count++;
        
        int current_tick = params->shared->sim_tick;
        printf("\n[SCHEDULER] Timer-ak aktibatu du (Sistemaren TICK: %d, Scheduler exekuzio: %d)\n", 
               current_tick, scheduler_tick_count);
        
        // ===== 1. WAITING TIME EGUNERATU =====
        for (pcb_t* p = params->ready_queue->head; p; p = p->next) {
            if (p->state == READY) {
                p->waiting_time++;
            }
        }
        
        pthread_mutex_lock(&cpu_sys->mutex);
        
        // Exekuzio fasea: HW thread-etan dauden prozesuak behatzen dira
        int completed_this_tick = 0;
        int instructions_this_tick = 0;
        int tick_based_procs = 0;
        int instruction_based_procs = 0;
        
        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                    
                    hw_thread_t* hw = &cpu_sys->cpus[c].cores[i].hw_threads[h];
                    pcb_t* cur = hw->current_process;
                    
                    if (cur && cur->state == RUNNING) {
                        // Instrukzio bidezko prozesuek memoria infoa behar dute; bestela, bukatutzat eman
                        if (cur->type == PROCESS_INSTRUCTION_BASED && (!cur->mm_info || !cur->mm_info->page_table)) {
                            printf("   PID=%d ERROREA: memoria informaziorik gabe (%s)\n",
                                   cur->pid, get_process_type_short(cur->type));
                            cur->state = TERMINATED;
                            cur->exit_code = -2;
                            queue_push(params->terminated_queue, cur);
                            hw->current_process = NULL;
                            completed_this_tick++;
                            continue;
                        }
                        // Kontatu prozesu motak
                        if (cur->type == PROCESS_TICK_BASED) {
                            tick_based_procs++;
                            
                            // TICK bidezko prozesua: denbora neurtzean aurreratzen da
                            cur->time_in_cpu++;
                            
                            // A) PROZESUA BUKATU (exec_time TICK-etatik)
                            if (cur->time_in_cpu >= cur->exec_time) {
                                printf("   PID=%d BUKATUTA (%s: %d/%d TICK)\n", 
                                       cur->pid, get_process_type_short(cur->type),
                                       cur->time_in_cpu, cur->exec_time);
                                cur->state = TERMINATED;
                                queue_push(params->terminated_queue, cur);
                                hw->current_process = NULL;
                                completed_this_tick++;
                                continue;
                            }
                            
                            // B) SAFETY QUANTUM
                            if (cur->time_in_cpu >= SAFETY_QUANTUM) {
                                printf("   PID=%d → READY (safety quantum, %d TICK)\n", 
                                       cur->pid, cur->time_in_cpu);
                                cur->state = READY;
                                queue_push(params->ready_queue, cur);
                                hw->current_process = NULL;
                                safety_preemptions++;
                                continue;
                            }
                            
                            // C) PROGRESOA ERAKUTSI
                            if (cur->time_in_cpu % 3 == 0) {
                                int progress = (cur->time_in_cpu * 100) / cur->exec_time;
                                if (progress < 100) {
                                    printf("   PID=%d exekutatzen (%s): %d/%d TICK (%d%%)\n",
                                           cur->pid, get_process_type_short(cur->type),
                                           cur->time_in_cpu, cur->exec_time, progress);
                                }
                            }
                        }
                        
                        // INSTRUKZIO bidezko prozesua: main loop-ean exekutatzen da
                        else if (cur->type == PROCESS_INSTRUCTION_BASED) {
                            instruction_based_procs++;
                            // Scheduler-ak ez du instrukzioak exekutatzen
                            // Main loop-ak exekutatzen ditu eta scheduler soilik bukaerak egiaztatu ditzake
                        }
                    }
                }
            }
        }
        
        // Informazioa erakutsi
        if (completed_this_tick == 0) {
            int running_count = 0;
            for (int c = 0; c < cpu_sys->cpu_kop; c++) {
                for (int i = 0; i < cpu_sys->core_kop; i++) {
                    for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                        if (cpu_sys->cpus[c].cores[i].hw_threads[h].current_process) {
                            running_count++;
                        }
                    }
                }
            }
            
            if (running_count > 0) {
                printf("   %d prozesu exekutatzen (TICK: %d, INSTR: %d, Instrukzioak: %d)\n", 
                       running_count, tick_based_procs, instruction_based_procs, instructions_this_tick);
            }
        } else {
            printf("   %d prozesu bukatu\n", completed_this_tick);
        }
        
        // Esleipen fasea: hutsik dauden HW thread-etan prozesuak esleitu
        int dispatched_this_tick = 0;
        
        for (int c = 0; c < cpu_sys->cpu_kop; c++) {
            for (int i = 0; i < cpu_sys->core_kop; i++) {
                for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                    
                    hw_thread_t* hw = &cpu_sys->cpus[c].cores[i].hw_threads[h];
                    
                    if (!hw->current_process) {
                        pcb_t* p = select_next_process(params->ready_queue, params->policy);
                        if (!p) continue;
                        
                        // Prozesua esleitu
                        p->state = RUNNING;
                        p->waiting_time = 0;
                        hw->current_process = p;
                        dispatched_this_tick++;
                        
                        // Konfiguratu prozesu motaren arabera (memoria birtuala vs denbora)
                        if (p->type == PROCESS_INSTRUCTION_BASED && p->mm_info) {
                            // INSTRUKZIO bidezko prozesua (memoria birtuala)
                            hw->mmu.ptbr = p->mm_info->ptbr;
                            hw->pc = p->pc = p->mm_info->code_start;
                            mmu_flush_tlb(&hw->mmu);
                            
                            printf("   PID=%d → HW %d-%d-%d (%s, PC=0x%06X, PTBR=0x%06X)\n", 
                                   p->pid, c, i, h, get_process_type_short(p->type),
                                   hw->pc, hw->mmu.ptbr);
                        } else {
                            // TICK bidezko prozesua
                            printf("   PID=%d → HW %d-%d-%d (%s, Prio=%d, Exec=%d %s)\n", 
                                   p->pid, c, i, h, get_process_type_short(p->type),
                                   p->priority, p->exec_time,
                                   (p->type == PROCESS_TICK_BASED) ? "TICK" : "instrukzio");
                        }
                    }
                }
            }
        }
        
        if (dispatched_this_tick == 0) {
            int ready_count = 0;
            for (pcb_t* p = params->ready_queue->head; p; p = p->next) {
                if (p->state == READY) ready_count++;
            }
            
            if (ready_count > 0) {
                printf("    READY ilaran %d prozesu, HW guztiak okupatuta\n", ready_count);
            }
        } else {
            printf("   %d prozesu esleitu\n", dispatched_this_tick);
        }
        
        // Blocked prozesuak kudeatu: I/O osoa simulatu (azpi probabilitatean)
        int io_completed = 0;
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
                
                printf("  PID=%d I/O amaitu → READY (%s)\n", 
                       to_unblock->pid, get_process_type_short(to_unblock->type));
                io_completed++;
            } else {
                prev = current;
                current = current->next;
            }
        }
        
        // Estatistikak eguneratu: 4 tick bakoitzean erakutsi (biltzen da)
        if (scheduler_tick_count % 4 == 0) {
            int current_tick = params->shared->sim_tick;
            printf("\n   TICK %d (Scheduler exekuzio %d) - SISTEMA ESTATISTIKAK:\n", 
                   current_tick, scheduler_tick_count);
            
            // Kontatu prozesu mota bakoitzeko
            int running_count = 0;
            int ready_count = 0;
            int blocked_count = 0;
            int terminated_count = 0;
            int tick_running = 0;
            int instruction_running = 0;
            
            // RUNNING kontatu
            for (int c = 0; c < cpu_sys->cpu_kop; c++) {
                for (int i = 0; i < cpu_sys->core_kop; i++) {
                    for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                        pcb_t* p = cpu_sys->cpus[c].cores[i].hw_threads[h].current_process;
                        if (p) {
                            running_count++;
                            if (p->type == PROCESS_TICK_BASED) tick_running++;
                            else instruction_running++;
                        }
                    }
                }
            }
            
            // READY kontatu
            for (pcb_t* p = params->ready_queue->head; p; p = p->next) {
                if (p->state == READY) ready_count++;
            }
            
            // BLOCKED kontatu
            for (pcb_t* p = params->blocked_queue->head; p; p = p->next) {
                if (p->state == BLOCKED) blocked_count++;
            }
            
            // TERMINATED kontatu
            for (pcb_t* p = params->terminated_queue->head; p; p = p->next) {
                if (p->state == TERMINATED) terminated_count++;
            }
            
            printf("    • Prozesuak: RUNNING=%d (TICK:%d, INSTR:%d), READY=%d\n",
                   running_count, tick_running, instruction_running, ready_count);
            printf("    • BLOCKED=%d, TERMINATED=%d\n", blocked_count, terminated_count);
            printf("    • Instrukzioak: %d (guztira: %d)\n",
                   instructions_this_tick, total_instructions_executed);
            
            // CPU erabilera kalkulatu
            int total_hw = cpu_sys->cpu_kop * cpu_sys->core_kop * cpu_sys->hw_thread_kop;
            if (total_hw > 0) {
                printf("    • CPU erabilera: %d/%d HW thread (%.0f%%)\n",
                       running_count,
                       total_hw,
                       (running_count * 100.0) / total_hw);
            }
            
            // Memoria estatistikak
            if (phys_mem.data != NULL) {
                printf("    • Memoria: %u frame libre (%u KB erabilgarri)\n",
                       phys_mem.free_frames, 
                       phys_mem.free_frames * PAGE_SIZE / 1024);
            }
            
            // Safety preemptions erakutsi
            if (safety_preemptions > 0) {
                printf("    • Safety preemptions: %d\n", safety_preemptions);
            }
        }
        
        pthread_mutex_unlock(&cpu_sys->mutex);
        
        // Pausa txiki bat simulazioa ikusteko
        usleep(200000);  // 0.2 segundo
    }
    
    printf("\n[SCHEDULER] %d scheduler exekuzio bukatu dira\n", scheduler_tick_count);
    printf("[SCHEDULER] Instrukzio guztira exekutatu: %d\n", total_instructions_executed);
    printf("[SCHEDULER] Safety preemptions guztira: %d\n", safety_preemptions);
    
    return NULL;
}

/* =======================================================
 * PROZESUEN ANALISIA (estatistikak kalkulatzeko)
 * ======================================================= */

void analyze_processes(SchedulerParams* params) {
    int total_waiting = 0;
    int total_turnaround = 0;
    int process_count = 0;
    int tick_based_count = 0;
    int instruction_based_count = 0;
    
    printf("\n=== PROZESUEN ANALISIA (TERMINATED prozesuak) ===\n");
    
    // Terminated prozesuak analizatu
    pcb_t* p = params->terminated_queue->head;
    while (p) {
        if (p->state == TERMINATED) {
            int turnaround_time = p->exec_time + p->waiting_time;
            total_waiting += p->waiting_time;
            total_turnaround += turnaround_time;
            process_count++;
            
            // Kontatu prozesu motak
            if (p->type == PROCESS_TICK_BASED) {
                tick_based_count++;
            } else {
                instruction_based_count++;
            }
            
            printf("PID=%d: %s, Exec=%d, Wait=%d, Turnaround=%d, Exit=%d\n",
                   p->pid, get_process_type_name(p->type),
                   p->exec_time, p->waiting_time, turnaround_time, p->exit_code);
        }
        p = p->next;
    }
    
    if (process_count > 0) {
        printf("\n=== ANALISI OROKORRA ===\n");
        printf("• Prozesu totalak: %d (TICK: %d, INSTRUKZIO: %d)\n", 
               process_count, tick_based_count, instruction_based_count);
        
        printf("\n• BATEZ BESTEKOAK:\n");
        printf("  - Waiting Time: %.2f tick\n", (float)total_waiting / process_count);
        printf("  - Turnaround Time: %.2f tick\n", (float)total_turnaround / process_count);
        
        // Bukaera tasa
        int total_active = process_count + queue_count(params->ready_queue) + 
                          queue_count(params->blocked_queue);
        if (total_active > 0) {
            float completion_rate = (process_count * 100.0) / total_active;
            printf("  - Bukaera tasa: %.1f%% (%d/%d)\n", 
                   completion_rate, process_count, total_active);
        }
    } else {
        printf("Ez dago terminated prozesurik analisirako.\n");
    }
}