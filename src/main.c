#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

#include "config.h"
#include "cpu.h"
#include "scheduler.h"
#include "process_generator.h"
#include "process_queue.h"
#include "pcb.h"
#include "memory.h"
#include "loader.h"
#include "hardware.h"
#include "execution.h"

// =======================================================
// MENU NAGUSIA - Koordinatzailerako funtzioak
// =======================================================

// Funtzio laguntzailea: prozesu motaren izena lortu
static const char* get_process_type_name(process_type_t type) {
    switch(type) {
        case PROCESS_TICK_BASED: return "TICK bidezkoa";
        case PROCESS_INSTRUCTION_BASED: return "INSTRUKZIO bidezkoa";
        default: return "EZEZAGUNA";
    }
}

void show_main_menu() {
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║      KERNEL SIMULATZAILEA - 2025/2026       ║\n");
    printf("║        Sistema Eragileak - Proiektua        ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ 1. Clock/Timer sinkronizazioa probatu       ║\n");
    printf("║ 2. Scheduler Menu Didaktikoa                ║\n");
    printf("║ 3. Simulazio automatikoa (TICK bidezkoak)   ║\n");
    printf("║ 4. Sistemaren informazioa                   ║\n");
    printf("║ 5. Memoria Birtuala (INSTRUKZIO bidezkoak)  ║\n");
    printf("║ 0. Irten                                    ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("Aukera hautatu: ");
}

// =======================================================
// 1. AUKERA: Clock/Timer sinkronizazioa
// =======================================================

void option_1_synchronization() {
    printf("\n=== 1. AUKERA: CLOCK/TIMER SINCRONIZAZIOA ===\n");
    printf("Erlojuaren eta timerren arteko sinkronizazioa\n");
    printf("frekuntza desberdinetan probatzen du.\n");
    printf("----------------------------------------------\n\n");
    
    // Datu partekatuak hasieratu
    SharedData shared = {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .cond2 = PTHREAD_COND_INITIALIZER,
        .cond_scheduler = PTHREAD_COND_INITIALIZER,
        .done = 0,
        .tenp_kop = TENP_KOP,
        .sim_running = 1,
        .sim_tick = 0,
        .scheduler_signal = 0 
    };
    
    ClockParams clock_params = {&shared, CLOCK_HZ};
    
    pthread_t clock_tid;
    pthread_t timer_tid[TENP_KOP];
    TimerParams timer_params[TENP_KOP];
    
    printf("Hariak sortzen...\n");
    
    // Clock hari bat sortu
    pthread_create(&clock_tid, NULL, clock_thread, &clock_params);
    usleep(100000);
    
    // Timer desberdinak konfiguratu
    timer_params[0].shared = &shared;
    timer_params[0].ticks_nahi = TIMER1_TICKS;
    timer_params[0].id = 1;
    timer_params[0].izena = "TIMER AZKARRA";
    timer_params[0].activate_scheduler = 0;
    
    timer_params[1].shared = &shared;
    timer_params[1].ticks_nahi = TIMER2_TICKS;
    timer_params[1].id = 2;
    timer_params[1].izena = "TIMER ERDIA";
    timer_params[1].activate_scheduler = 0;
    
    timer_params[2].shared = &shared;
    timer_params[2].ticks_nahi = TIMER3_TICKS;
    timer_params[2].id = 3;
    timer_params[2].izena = "TIMER MANTSOA";
    timer_params[2].activate_scheduler = 0;
    
    // Timer guztiak sortu
    for (int i = 0; i < TENP_KOP; i++) {
        pthread_create(&timer_tid[i], NULL, timer_thread, &timer_params[i]);
        usleep(50000);
    }
    
    printf("\n Timer motak:\n");
    printf("- TIMER AZKARRA: %d tick (%.2f Hz)\n", TIMER1_TICKS, (double)CLOCK_HZ/TIMER1_TICKS);
    printf("- TIMER ERDIA: %d tick (%.2f Hz)\n", TIMER2_TICKS, (double)CLOCK_HZ/TIMER2_TICKS);
    printf("- TIMER MANTSOA: %d tick (%.2f Hz)\n", TIMER3_TICKS, (double)CLOCK_HZ/TIMER3_TICKS);
    
    printf("\nSistema 15 segundoz exekutatzen...\n");
    printf("Ctrl+C sakatu aurretik amaitzeko.\n\n");
    
    for (int i = 15; i > 0; i--) {
        printf("\rGeratzen diren segundoak: %2d", i);
        fflush(stdout);
        sleep(1);
    }
    
    printf("\n\nProba amaitzen...\n");
    
    shared.sim_running = 0;
    shared.done = 1;
    pthread_mutex_lock(&shared.mutex);
    pthread_cond_broadcast(&shared.cond);
    pthread_cond_broadcast(&shared.cond2);
    pthread_cond_broadcast(&shared.cond_scheduler);
    pthread_mutex_unlock(&shared.mutex);
    
    printf("Clock hariaren amaiera itxaroten...\n");
    pthread_join(clock_tid, NULL);
    
    printf("Timer hariek amaiera itxaroten...\n");
    for (int i = 0; i < TENP_KOP; i++) {
        pthread_join(timer_tid[i], NULL);
    }
    
    printf("Sinkronizazio proba osatuta.\n");
}

// =======================================================
// 2. AUKERA: Scheduler Menu Didaktikoa
// =======================================================

// Función ya definida arriba como static

void print_ready_queue_menu(process_queue_t* q) {
    printf("\n--- READY QUEUE (Prest dauden prozesuak) ---\n");
    if (!q->head) {
        printf(" (hutsik)\n");
        return;
    }
    for (pcb_t* p = q->head; p; p = p->next) {
        printf(" PID=%d | Mota: %-18s | Exec: %-4d | CPU: %-3d | Wait: %-3d | Prio: %d\n",
               p->pid, get_process_type_name(p->type), 
               p->exec_time, p->time_in_cpu, p->waiting_time, p->priority);
    }
}

void print_blocked_queue_menu(process_queue_t* q) {
    printf("\n--- BLOCKED QUEUE (I/O itxaroten) ---\n");
    if (!q->head) {
        printf(" (hutsik)\n");
        return;
    }
    for (pcb_t* p = q->head; p; p = p->next) {
        printf(" PID=%d | Mota: %s | Exec=%d | CPU=%d\n",
               p->pid, get_process_type_name(p->type),
               p->exec_time, p->time_in_cpu);
    }
}

void print_running_queue_menu(cpu_system_t* cpu_sys) {
    printf("\n--- RUNNING PROZESUAK (Exekutatzen ari direnak) ---\n");
    int found = 0;

    for (int c = 0; c < cpu_sys->cpu_kop; c++) {
        for (int i = 0; i < cpu_sys->core_kop; i++) {
            for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                hw_thread_t* hw = &cpu_sys->cpus[c].cores[i].hw_threads[h];
                if (hw->current_process) {
                    pcb_t* p = hw->current_process;
                    printf(" PID=%d | CPU:%d-CORE:%d-HW:%d | Mota: %s | %d/%d\n",
                           p->pid, c, i, h, get_process_type_name(p->type),
                           p->time_in_cpu, p->exec_time);
                    found = 1;
                }
            }
        }
    }
    if (!found) printf(" (ez dago exekutatzen ari den prozesurik)\n");
}

void option_2_didactic_menu() {
    printf("\n=== 2. AUKERA: SCHEDULER MENU DIDAKTIKOA ===\n");
    printf("Schedulerraren funtzionamendua pausoz pauso\n");
    printf("Prozesu motak: TICK bidezkoak eta INSTRUKZIO bidezkoak\n");
    printf("------------------------------------------------------\n");
    
    SharedData shared;
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond, NULL);
    pthread_cond_init(&shared.cond2, NULL);
    pthread_cond_init(&shared.cond_scheduler, NULL);
    shared.done = 0;
    shared.tenp_kop = 0;
    shared.sim_running = 1;
    shared.sim_tick = 0;
    shared.scheduler_signal = 0;
    
    cpu_system_t cpu_sys;
    mmu_logs_enabled = 0;  // TICK soilik moduan MMU log-ak ez erakutsi
    cpu_system_init(&cpu_sys);
    
    process_queue_t ready_q, blocked_q, terminated_q;
    queue_init(&ready_q);
    queue_init(&blocked_q);
    queue_init(&terminated_q);
    
    SchedulerParams sched = {
        .shared = &shared,
        .ready_queue = &ready_q,
        .blocked_queue = &blocked_q,
        .terminated_queue = &terminated_q,
        .cpu_sys = &cpu_sys,
        .policy = POLICY_RULETA_AVANZATUA
    };
    
    pthread_t sched_thread;
    pthread_create(&sched_thread, NULL, scheduler, &sched);
    
    usleep(200000);
    
    int pid = 1;
    int opt;
    int tick_count = 0;
    
    while (1) {
        printf("\n══════════════════════════════════════════════\n");
        printf("MENU DIDAKTIKOA - TICK: %d\n", tick_count);
        printf("══════════════════════════════════════════════\n");
        printf("1. Sortu TICK bidezko prozesua (1-2. zatia)    \n");
        printf("2. Sortu INSTRUKZIO bidezko prozesua (3. zatia)\n");
        printf("3. TIMER aktibatu (Scheduler-a aktibatzeko)    \n");
        printf("4. RUNNING -> BLOCKED (I/O eskaera)            \n");
        printf("5. BLOCKED -> READY (I/O amaiera)              \n");
        printf("6. Erakutsi prozesu ilarak                     \n");
        printf("7. Erakutsi CPU egoera                         \n");
        printf("8. Prozesu bat bukatu (FORZATU)                \n");
        printf("0. Menu nagusira itzuli                        \n");
        printf("══════════════════════════════════════════════\n");
        printf("Aukeratu: ");
        
        if (scanf("%d", &opt) != 1) {
            while (getchar() != '\n');
            printf("\n Sarrera okerra. Zenbaki bat sartu.\n");
            continue;
        }
        
        if (opt == 0) break;
        
        switch(opt) {
            case 1: {
                pcb_t* p = pcb_create(pid++, rand() % 2);
                p->state = READY;
                p->type = PROCESS_TICK_BASED;
                p->exec_time = 3 + rand() % 8;
                queue_push(&ready_q, p);
                printf("\n TICK bidezko prozesua: PID=%d (Exec=%d TICK, Prio=%d)\n", 
                       p->pid, p->exec_time, p->priority);
                printf("   (Amaituko da %d TICK-en ondoren)\n", p->exec_time);
                break;
            }
            
            case 2: {
                pcb_t* p = pcb_create(pid++, rand() % 2);
                p->state = READY;
                p->type = PROCESS_INSTRUCTION_BASED;
                p->exec_time = 5 + rand() % 10;
                p->pc = 0x000000;
                queue_push(&ready_q, p);
                printf("\n INSTRUKZIO bidezko prozesua: PID=%d (Instrukzio max: %d)\n", 
                       p->pid, p->exec_time);
                printf("   (Amaituko da EXIT agindua aurkitzean edo %d instrukzio ondoren)\n", 
                       p->exec_time);
                printf("   Oharra: Memoria birtuala simulatzen, ez da programa benetan kargatzen\n");
                break;
            }
            
            case 3: {
                tick_count++;
                printf("\n══════════════════════════════════════════════\n");
                printf("TIMER aktibatzen... (TICK #%d)\n", tick_count);
                printf("══════════════════════════════════════════════\n");
                
                pthread_mutex_lock(&shared.mutex);
                shared.scheduler_signal = 1;
                pthread_cond_signal(&shared.cond_scheduler);
                pthread_mutex_unlock(&shared.mutex);
                
                usleep(500000);
                
                printf("\nSakatu Enter jarraitzeko...");
                while (getchar() != '\n');
                getchar();
                break;
            }
            
            case 4: {
                int aurkitua = 0;
                for (int c = 0; c < cpu_sys.cpu_kop; c++) {
                    for (int i = 0; i < cpu_sys.core_kop; i++) {
                        for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                            hw_thread_t* hw = &cpu_sys.cpus[c].cores[i].hw_threads[h];
                            if (hw->current_process) {
                                pcb_t* p = hw->current_process;
                                hw->current_process = NULL;
                                p->state = BLOCKED;
                                queue_push(&blocked_q, p);
                                printf("\n I/O eskaera: PID=%d RUNNING -> BLOCKED (Mota: %s)\n", 
                                       p->pid, get_process_type_name(p->type));
                                aurkitua = 1;
                                goto io_done;
                            }
                        }
                    }
                }
                io_done:
                if (!aurkitua) printf("\n Ez dago RUNNING prozesurik\n");
                break;
            }
            
            case 5: {
                pcb_t* p = queue_pop(&blocked_q);
                if (p) {
                    p->state = READY;
                    queue_push(&ready_q, p);
                    printf("\n I/O amaiera: PID=%d BLOCKED -> READY (Mota: %s)\n", 
                           p->pid, get_process_type_name(p->type));
                } else {
                    printf("\n Ez dago BLOCKED prozesurik\n");
                }
                break;
            }
            
            case 6: {
                printf("\n=== PROZESU ILARAK ===\n");
                print_ready_queue_menu(&ready_q);
                print_blocked_queue_menu(&blocked_q);
                
                printf("\n--- TERMINATED QUEUE (Bukatutako prozesuak) ---\n");
                if (!terminated_q.head) {
                    printf(" (hutsik)\n");
                } else {
                    for (pcb_t* p = terminated_q.head; p; p = p->next) {
                        printf(" PID=%d | Mota: %s | exec=%d | cpu=%d\n",
                               p->pid, get_process_type_name(p->type),
                               p->exec_time, p->time_in_cpu);
                    }
                }
                break;
            }
            
            case 7: {
                print_running_queue_menu(&cpu_sys);
                break;
            }
            
            case 8: {
                int aurkitua = 0;
                for (int c = 0; c < cpu_sys.cpu_kop; c++) {
                    for (int i = 0; i < cpu_sys.core_kop; i++) {
                        for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                            hw_thread_t* hw = &cpu_sys.cpus[c].cores[i].hw_threads[h];
                            if (hw->current_process) {
                                pcb_t* p = hw->current_process;
                                hw->current_process = NULL;
                                p->state = TERMINATED;
                                queue_push(&terminated_q, p);
                                printf("\n Prozesua bukatu (FORZATU): PID=%d (Mota: %s)\n", 
                                       p->pid, get_process_type_name(p->type));
                                aurkitua = 1;
                                goto force_done;
                            }
                        }
                    }
                }
                force_done:
                if (!aurkitua) printf("\n Ez dago RUNNING prozesurik\n");
                break;
            }
                
            default:
                printf("\n Aukera okerra. 0 eta 8 artean aukeratu.\n");
        }
    }
    
    shared.done = 1;
    shared.sim_running = 0;
    pthread_mutex_lock(&shared.mutex);
    pthread_cond_signal(&shared.cond_scheduler);
    pthread_mutex_unlock(&shared.mutex);
    
    printf("\nScheduler hariaren amaiera itxaroten...\n");
    pthread_join(sched_thread, NULL);
    
    pthread_mutex_destroy(&shared.mutex);
    pthread_cond_destroy(&shared.cond);
    pthread_cond_destroy(&shared.cond2);
    pthread_cond_destroy(&shared.cond_scheduler);
    
    printf("\n Menu didaktikoa amaituta.\n");
}

// =======================================================
// 3. AUKERA: Simulazio automatikoa (TICK bidezkoak)
// =======================================================

void option_3_automatic_simulation() {
    printf("\n=== 3. AUKERA: SIMULAZIO AUTOMATIKOA (TICK bidezkoak) ===\n");
    printf("TICK bidezko prozesuen simulazioa (1-2. zatia)\n");
    printf("Prozesuak TICK kopuru baten arabera exekutatzen dira\n");
    printf("----------------------------------------------------\n\n");
    
    printf("Sistemaren konfigurazioa:\n");
    printf("- Clock: %.1f Hz\n", CLOCK_HZ);
    printf("- Timerrak: %d\n", TENP_KOP);
    printf("- Politika: Ruleta Aurreratua\n");
    printf("- CPU: %d, Core: %d, HW Thread: %d\n", 
           CPU_KOP, CORE_KOP, HW_THREAD_KOP);
    printf("\nSimulazioa abiarazten...\n\n");
    
    SharedData shared;
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond, NULL);
    pthread_cond_init(&shared.cond2, NULL);
    pthread_cond_init(&shared.cond_scheduler, NULL);
    shared.done = 0;
    shared.tenp_kop = 1;
    shared.sim_running = 1;
    shared.sim_tick = 0;
    shared.scheduler_signal = 0;
    
    cpu_system_t cpu_sys;
    mmu_logs_enabled = 0;  // TICK aukeran MMU log-ak ez erakutsi
    cpu_system_init(&cpu_sys);
    
    process_queue_t ready_q, blocked_q, terminated_q;
    queue_init(&ready_q);
    queue_init(&blocked_q);
    queue_init(&terminated_q);
    
    printf("Prozesu hasierakoak sortzen (TICK bidezkoak)...\n");
    for (int i = 0; i < 5; i++) {
        pcb_t* p = pcb_create(i+1, rand() % 2);
        p->state = READY;
        p->type = PROCESS_TICK_BASED;
        p->exec_time = 3 + rand() % 8;
        queue_push(&ready_q, p);
        printf("  PID=%d sortuta (TICK bidezkoa, Exec=%d TICK)\n", p->pid, p->exec_time);
    }
    
    SchedulerParams sched_params = {
        .shared = &shared,
        .ready_queue = &ready_q,
        .blocked_queue = &blocked_q,
        .terminated_queue = &terminated_q,
        .cpu_sys = &cpu_sys,
        .policy = POLICY_RULETA_AVANZATUA
    };
    
    pthread_t sched_thread;
    pthread_create(&sched_thread, NULL, scheduler, &sched_params);
    
    ClockParams clock_params = {&shared, CLOCK_HZ};
    pthread_t clock_tid;
    pthread_create(&clock_tid, NULL, clock_thread, &clock_params);
    
    TimerParams timer_params;
    timer_params.shared = &shared;
    timer_params.ticks_nahi = 2;
    timer_params.id = 1;
    timer_params.izena = "SCHEDULER TIMER";
    timer_params.activate_scheduler = 1;
    
    pthread_t timer_thread_id;
    pthread_create(&timer_thread_id, NULL, timer_thread, &timer_params);
    
    usleep(500000);
    
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║      SIMULAZIOA MARTXAN                  ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Iraupena: 20 tick                        ║\n");
    printf("║ Prozesu mota: TICK bidezkoak bakarrik   ║\n");
    printf("║ Scheduler periodoa: %d tick             ║\n", timer_params.ticks_nahi);
    printf("╚══════════════════════════════════════════╝\n\n");
    
    int tick_max = 20;
    int last_shown_tick = 0;
    int last_executed_tick = -1;
    
    while (shared.sim_running && shared.sim_tick < tick_max) {
        int current_tick = shared.sim_tick;
        
        // Itxaron tick berri bat egon arte (clock thread-ak eguneratzen du)
        if (current_tick == last_executed_tick) {
            usleep(5000);  // 5ms itxaron
            continue;
        }
        
        last_executed_tick = current_tick;
        
        // === EXEKUZIO: Prozesu TICK-based bakoitzak tick bat aurreratzen du ===
        pthread_mutex_lock(&cpu_sys.mutex);
        
        for (int c = 0; c < cpu_sys.cpu_kop; c++) {
            for (int i = 0; i < cpu_sys.core_kop; i++) {
                for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                    hw_thread_t* hw = &cpu_sys.cpus[c].cores[i].hw_threads[h];
                    pcb_t* p = hw->current_process;
                    
                    if (p && p->state == RUNNING && p->type == PROCESS_TICK_BASED) {
                        // TICK bat aurreratu
                        p->time_in_cpu++;
                        
                        // Egiaztatu ea bukatu den
                        if (p->time_in_cpu >= p->exec_time) {
                            printf("[TICK] PID %d: bukatu da (%d/%d TICK)\n", 
                                   p->pid, p->time_in_cpu, p->exec_time);
                            hw->current_process = NULL;
                            p->state = TERMINATED;
                            queue_push(&terminated_q, p);
                        }
                    }
                }
            }
        }
        
        pthread_mutex_unlock(&cpu_sys.mutex);
        
        // === ERAKUSTE FASEA ===
        if (current_tick != last_shown_tick) {
            last_shown_tick = current_tick;
        
            printf("\n══════════════════════════════════════════════\n");
            printf(" TICK #%d - SIMULAZIO AUTOMATIKOA (TICK bidezkoak)\n", current_tick);
            printf("══════════════════════════════════════════════\n");
            
            // EKINTZA ALEATORIOAK
            if (rand() % 100 < 25) {
                pcb_t* p = pcb_create(100 + current_tick, rand() % 2);
                p->state = READY;
                p->type = PROCESS_TICK_BASED;
                p->exec_time = 2 + rand() % 6;
                queue_push(&ready_q, p);
                printf("\n[EKINTZA] TICK bidezko prozesu berria: PID=%d (Exec=%d TICK)\n", 
                       p->pid, p->exec_time);
            }
            
            if (rand() % 100 < 20) {
                pthread_mutex_lock(&cpu_sys.mutex);
                for (int c = 0; c < cpu_sys.cpu_kop; c++) {
                    for (int i = 0; i < cpu_sys.core_kop; i++) {
                        for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                            hw_thread_t* hw = &cpu_sys.cpus[c].cores[i].hw_threads[h];
                            if (hw->current_process) {
                                pcb_t* p = hw->current_process;
                                hw->current_process = NULL;
                                p->state = BLOCKED;
                                queue_push(&blocked_q, p);
                                printf("\n[EKINTZA] I/O eskaera: PID=%d → BLOCKED\n", p->pid);
                                pthread_mutex_unlock(&cpu_sys.mutex);
                                goto io_action_done;
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&cpu_sys.mutex);
                io_action_done:;
            }
            
            if (rand() % 100 < 30 && blocked_q.head) {
                pcb_t* p = queue_pop(&blocked_q);
                if (p) {
                    p->state = READY;
                    queue_push(&ready_q, p);
                    printf("\n[EKINTZA] I/O amaiera: PID=%d → READY\n", p->pid);
                }
            }
            
            // EGOERA OROKORRA
            printf("\n[EGOERA OROKORRA]\n");
            
            int running_count = 0;
            
            // Prozesu RUNNING-ak kontatu eta erakutsi
            for (int c = 0; c < cpu_sys.cpu_kop; c++) {
                for (int i = 0; i < cpu_sys.core_kop; i++) {
                    for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                        pcb_t* p = cpu_sys.cpus[c].cores[i].hw_threads[h].current_process;
                        if (p) {
                            running_count++;
                            int progress = (p->time_in_cpu * 100) / p->exec_time;
                            printf("  • PID=%d (TICK): HW %d-%d-%d | %d/%d TICK (%d%%)\n",
                                   p->pid, c, i, h, p->time_in_cpu, p->exec_time, progress);
                        }
                    }
                }
            }
            
            printf("\n[LABURPENA]\n");
            printf("  RUNNING: %d (TICK:%d) | READY: %d | BLOCKED: %d | TERMINATED: %d\n",
                   running_count, running_count,
                   queue_count(&ready_q),
                   queue_count(&blocked_q),
                   queue_count(&terminated_q));
        }
        
        // Hariak exekutatu eta erlojua aurreratu ahal izateko pixka bat lo egin
        usleep(300000);  // 300ms
    }
    
    printf("\n══════════════════════════════════════════════\n");
    printf(" SIMULAZIOA AMAITUTA (TICK bidezkoak)\n");
    printf("══════════════════════════════════════════════\n\n");
    
    printf(" FINAL EGOERA:\n");
    printf("  Tick guztiak: %d\n", shared.sim_tick);
    printf("  Prozesu totalak: %d\n", 
           queue_count(&ready_q) + queue_count(&blocked_q) + queue_count(&terminated_q));
    printf("  Prozesu bukatuak: %d\n", queue_count(&terminated_q));
    printf("  READY egoeran: %d\n", queue_count(&ready_q));
    printf("  BLOCKED egoeran: %d\n", queue_count(&blocked_q));
    
    int total_procesos = queue_count(&ready_q) + queue_count(&blocked_q) + queue_count(&terminated_q);
    if (total_procesos > 0) {
        printf("  Bukaera tasa: %.1f%%\n", 
               (queue_count(&terminated_q) * 100.0) / total_procesos);
    }
    
    shared.sim_running = 0;
    shared.done = 1;
    
    pthread_mutex_lock(&shared.mutex);
    shared.scheduler_signal = 1;
    pthread_cond_broadcast(&shared.cond_scheduler);
    pthread_cond_broadcast(&shared.cond);
    pthread_cond_broadcast(&shared.cond2);
    pthread_mutex_unlock(&shared.mutex);
    
    pthread_join(sched_thread, NULL);
    pthread_join(timer_thread_id, NULL);
    pthread_join(clock_tid, NULL);
    
    printf("\n TICK bidezko simulazioa ondo amaituta.\n");
}

// =======================================================
// 4. AUKERA: Sistemaren informazioa
// =======================================================

void option_4_system_info() {
    printf("\n=== SISTEMAREN INFORMAZIOA ===\n");
    printf("Kernel Simulatzailea - Sistema Eragileak 2025/2026\n");
    printf("\nOsagai inplementatuak:\n");
    printf(" 1. ZATIA - Sistemaren arkitektura (1 puntu):\n");
    printf("    • Clock (sistemaren erlojua) - %.1f Hz\n", CLOCK_HZ);
    printf("    • Timer anitzak (%d) frekuntza desberdinetan\n", TENP_KOP);
    printf("    • CPU sistema: %d CPU × %d Core × %d HW Thread\n",
           CPU_KOP, CORE_KOP, HW_THREAD_KOP);
    
    printf("\n 2. ZATIA - Planifikatzailea (3 puntu):\n");
    printf("    • Prozesuen sortzailea (Process Generator)\n");
    printf("    • Scheduler (Ruleta Aurreratua politika)\n");
    printf("    • Prozesuen ilarak (Ready, Blocked, Terminated)\n");
    printf("    • PCB (Process Control Block)\n");
    printf("    • PROZESU MOTAK:\n");
    printf("      - TICK bidezkoak: TICK kopuru baten arabera exekutatzen dira\n");
    printf("      - INSTRUKZIO bidezkoak: Instrukzioen arabera exekutatzen dira\n");
    
    printf("\n 3. ZATIA - Memoriaren kudeatzailea (2 puntu):\n");
    printf("    • Memoria birtualaren kudeaketa\n");
    printf("    • MMU (Memory Management Unit)\n");
    printf("    • TLB (Translation Lookaside Buffer)\n");
    printf("    • Page Table-ak eta helbide-itzulpena\n");
    printf("    • Programa exekuzioa (ld, st, add, exit)\n");
    printf("    • Memoria fisikoa: 16 MB, 4 KB-ko frame-ak\n");
    printf("    • Kernelaren memoria: 4 MB erreserbatuta\n");
    
    printf("\nEragiketa moduak:\n");
    printf("1. Sinkronizazio proba (Clock/Timer)\n");
    printf("2. Menu didaktikoa (Scheduler interaktiboa)\n");
    printf("3. Simulazio automatikoa (TICK bidezkoak)\n");
    printf("4. Sistemaren informazioa (mezu hau)\n");
    printf("5. Memoria Birtuala (INSTRUKZIO bidezkoak)\n");
    
    printf("\nProiektu osoa inplementatuta (3 zatia):\n");
    printf("• Arkitektura + Planifikatzailea + Memoria kudeaketa\n");
    printf("• Sistema hibridoa: TICK eta INSTRUKZIO bidezko prozesuak\n");
    printf("• Dokumentazioa: Diseinu eta inplementazio zehaztasunak\n");
    
    printf("\nKodea: ANSI C\n");
    printf("Sistema: UNIX motako sistema eragilea\n");
    printf("Garapena: Git bertsio-kontrola\n");
}

// =======================================================
// 5. AUKERA: Memoria Birtuala (INSTRUKZIO bidezkoak)
// =======================================================

void option_5_memory_virtual_simulation() {
    mmu_logs_enabled = 1;  // MMU log-ak gaitu INSTRUKZIO simulaziorako
    printf("\n=== 5. AUKERA: MEMORIA BIRTUALAREN SIMULAZIOA ===\n");
    printf("INSTRUKZIO bidezko prozesuen simulazioa (3. zatia)\n");
    printf("Prozesuak instrukzioen arabera exekutatzen dira\n");
    printf("--------------------------------------------------\n\n");
    
    printf("FAZE 1: Sistemaren arkitektura hedatu\n");
    printf("--------------------------------------\n");
    
    // 1. Memoria fisikoa hasieratu
    printf("1. Memoria fisikoa hasieratzen...\n");
    physical_memory_init();
    
    // 2. Programa fitxategiak sortu (simulazioa)
    // Programak kargatu elf karpetatik
    const char* elf_files[] = {
        "elf/prog000.elf",
        "elf/prog001.elf", 
        "elf/prog002.elf",
        "elf/prog003.elf",
        "elf/prog004.elf"
    };
    
    program_t* programs[5];
    int total_instructions = 0;
    
    for (int i = 0; i < 5; i++) {
        programs[i] = load_program_from_file(elf_files[i]);
        if (programs[i]) {
            total_instructions += programs[i]->code_size;
            printf("  - %s: %d instrukzio (0x%06X - 0x%06X)\n", 
                   elf_files[i], programs[i]->code_size,
                   programs[i]->code_start,
                   programs[i]->code_start + (programs[i]->code_size * 4) - 1);
        } else {
            printf("  - ERROREA: Ezin izan da %s kargatu\n", elf_files[i]);
            return;
        }
    }
    
    printf("   Guztira: %d instrukzio\n", total_instructions);
    
    printf("\nFAZE 2: Prozesuak sortu\n");
    printf("------------------------\n");
    
    // 3. CPU sistema hasieratu
    printf("3. CPU eta hardware hasieratzen...\n");
    cpu_system_t cpu_sys;
    cpu_system_init(&cpu_sys);
    
    // Prozesuak gordetzeko ilarak sortu
    process_queue_t ready_q, blocked_q, terminated_q;
    queue_init(&ready_q);
    queue_init(&blocked_q);
    queue_init(&terminated_q);
    
    // 5. 5 prozesu sortu (INSTRUCTION-BASED soilik)
    printf("4. 5 prozesu ELF-etik sortzen...\n");
    int processes_created = 0;
    
    for (int i = 0; i < 5; i++) {
        // Prozesuaren tamainaren arabera prioridad esleitu
        // Prozesu laburrak (< 15 instr): garrantzitsua, scheduler-an gehiago hautatzeko aukera
        // Prozesu luzeak: normala, ez urgentea
        int priority = (programs[i]->code_size < 15) ? 1 : 0;
        
        pcb_t* proc = create_process_from_program(i + 1, priority, programs[i]);
        if (proc) {
            proc->type = PROCESS_INSTRUCTION_BASED;
            proc->state = READY;
            proc->exec_time = programs[i]->code_size;
            proc->pc = programs[i]->code_start;
            queue_push(&ready_q, proc);
            processes_created++;
            const char* prio_text = (priority == 1) ? "GARRANTZITSUA (x3)" : "NORMALA (x1)";
            printf("   PID=%d: %d instrukzio, Prioridad: %s\n", proc->pid, proc->exec_time, prio_text);
        } else {
            printf("   ERROREA: PID=%d ezin izan da sortu\n", i + 1);
        }
    }
    
    printf("\nFAZE 3: Simulazioa\n");
    printf("------------------\n");
    
    // Scheduler eta sinkronizazioa ezarri
    SharedData shared;
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond, NULL);
    pthread_cond_init(&shared.cond2, NULL);
    pthread_cond_init(&shared.cond_scheduler, NULL);
    shared.done = 0;
    shared.tenp_kop = 1;
    shared.sim_running = 1;
    shared.sim_tick = 0;
    shared.scheduler_signal = 0;
    
    SchedulerParams sched_params = {
        .shared = &shared,
        .ready_queue = &ready_q,
        .blocked_queue = &blocked_q,
        .terminated_queue = &terminated_q,
        .cpu_sys = &cpu_sys,
        .policy = POLICY_RULETA_AVANZATUA
    };
    
    pthread_t sched_thread;
    pthread_create(&sched_thread, NULL, scheduler, &sched_params);
    
    // 7. Clock eta Timer sortu
    ClockParams clock_params = {&shared, CLOCK_HZ};
    pthread_t clock_tid;
    pthread_create(&clock_tid, NULL, clock_thread, &clock_params);
    
    TimerParams timer_params;
    timer_params.shared = &shared;
    timer_params.ticks_nahi = 2;  // Scheduler 2 tick-eko aktibatzen da - execution tick bakoitzean
    timer_params.id = 1;
    timer_params.izena = "MEMORIA TIMER";
    timer_params.activate_scheduler = 1;
    
    pthread_t timer_thread_id;
    pthread_create(&timer_thread_id, NULL, timer_thread, &timer_params);
    
    usleep(50000);
    
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║      MEMORIA BIRTUALAREN SIMULAZIOA         ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ [SCHEDULER] Hasieratuta                      ║\n");
    printf("║ Prozesu motak: INSTRUKZIO bidezkoak soilak  ║\n");
    printf("║ Prozesu kopurua: %d                         ║\n", processes_created);
    printf("║ [MEMORY] Frame libre: %u                    ║\n", phys_mem.free_frames);
    printf("║ Scheduler periodoa: 2 tick                 ║\n");
    printf("║ Execution: TICK bakoitzean                  ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    
    // HW thread-ak exekutatzen dira tick bakoitzean, scheduler asignatzen du 2 tick-eko
    int tick_max = 300;
    int last_shown_tick = 0;
    int last_executed_tick = -1;
    
    while (shared.sim_running && shared.sim_tick < tick_max) {
        int current_tick = shared.sim_tick;
        
        // Itxaron tick berri bat egon arte (clock thread-ak eguneratzen du)
        if (current_tick == last_executed_tick) {
            usleep(5000);  // 5ms itxaron
            continue;
        }
        
        last_executed_tick = current_tick;
        
        // === EXEKUZIO: HW thread-ek instrukzioak exekutatzen dituzte ===
        pthread_mutex_lock(&cpu_sys.mutex);
        
        for (int c = 0; c < cpu_sys.cpu_kop; c++) {
            for (int i = 0; i < cpu_sys.core_kop; i++) {
                for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                    hw_thread_t* hw = &cpu_sys.cpus[c].cores[i].hw_threads[h];
                    pcb_t* p = hw->current_process;
                    
                    if (p && p->state == RUNNING && p->type == PROCESS_INSTRUCTION_BASED) {
                        // Instrukzio bat exekutatu
                        int result = execute_step(hw, p);
                        
                        if (result > 0) {
                            p->time_in_cpu++;
                            // Scheduler-ak egiaztatu instrukzioak bukaturik dauden
                        } else if (result == 0) {
                            // EXIT agindua - HW thread askatu eta ilaran jarri scheduler-entzat
                            p->time_in_cpu++;
                            hw->current_process = NULL;
                            queue_push(&terminated_q, p);  // Scheduler-ak egiaztatu eta TERMINATED jarriko du
                            mmu_flush_tlb(&hw->mmu);
                        } else if (result < 0) {
                            // Errorea - markar exit code eta ilaran jarri
                            p->exit_code = -1;
                            hw->current_process = NULL;
                            queue_push(&terminated_q, p);  // Scheduler-ak TERMINATED jarriko du
                            mmu_flush_tlb(&hw->mmu);
                        }
                    }
                }
            }
        }
        
        pthread_mutex_unlock(&cpu_sys.mutex);
        
        // === ERAKUSTE FASEA ===
        if (current_tick != last_shown_tick) {
            last_shown_tick = current_tick;
        
            printf("\n══════════════════════════════════════════════\n");
            printf(" TICK #%d - MEMORIA BIRTUALA (INSTRUKZIO)\n", current_tick);
            printf("══════════════════════════════════════════════\n");
            
            // EGOERA OROKORRA
            printf("\n[EGOERA OROKORRA]\n");
        
        int running_count = 0;
        int instruction_running = 0;
        
        // Prozesu RUNNING-ak kontatu eta erakutsi
        for (int c = 0; c < cpu_sys.cpu_kop; c++) {
            for (int i = 0; i < cpu_sys.core_kop; i++) {
                for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                    pcb_t* p = cpu_sys.cpus[c].cores[i].hw_threads[h].current_process;
                    if (p) {
                        running_count++;
                        instruction_running++;
                        int progress = (p->time_in_cpu * 100) / p->exec_time;
                        printf("  • PID=%d (INSTR): HW %d-%d-%d | %d/%d instrukzio (%d%%) | PC=0x%06X\n",
                               p->pid, c, i, h, p->time_in_cpu, p->exec_time, progress, p->pc);
                    }
                }
            }
        }
        
        printf("\n[LABURPENA]\n");
        printf("  RUNNING: %d (INSTR:%d) | READY: %d | BLOCKED: %d | TERMINATED: %d\n",
               running_count, instruction_running,
               queue_count(&ready_q),
               queue_count(&blocked_q),
               queue_count(&terminated_q));
        
        if (phys_mem.data != NULL) {
            printf("  Memoria: %u frame libre (%u KB erabilgarri)\n",
                   phys_mem.free_frames, 
                   phys_mem.free_frames * PAGE_SIZE / 1024);
        }
        }
        
        // Amaiera baldintzak - 5 prozesu ELF guztiak bukatu arte
        if (queue_count(&terminated_q) >= 5) {
            printf("\n[OHARRA] Prozesu guztiak bukatu dira. Simulazioa amaitzen...\n");
            break;
        }
    }
    
    // 11. SIMULAZIOA AMAITU
    printf("\n══════════════════════════════════════════════\n");
    printf(" SIMULAZIOA AMAITUTA (INSTRUKZIO bidezkoak)\n");
    printf("══════════════════════════════════════════════\n\n");
    
    printf(" FINAL EGOERA:\n");
    printf("  Tick guztiak: %d\n", shared.sim_tick);
    printf("  Prozesu totalak: %d\n", 
           queue_count(&ready_q) + queue_count(&blocked_q) + queue_count(&terminated_q));
    
    // Analisia prozesu motaren arabera
    int tick_terminated = 0;
    int instruction_terminated = 0;
    
    pcb_t* p = terminated_q.head;
    while (p) {
        if (p->type == PROCESS_TICK_BASED) tick_terminated++;
        else instruction_terminated++;
        p = p->next;
    }
    
    printf("  Prozesu bukatuak: %d (TICK:%d, INSTRUKZIO:%d)\n", 
           queue_count(&terminated_q), tick_terminated, instruction_terminated);
    printf("  READY egoeran: %d\n", queue_count(&ready_q));
    printf("  BLOCKED egoeran: %d\n", queue_count(&blocked_q));
    
    // Memoria egoera
    if (phys_mem.data != NULL) {
        printf("  Memoria: %u frame libre (%u KB erabilgarri)\n",
               phys_mem.free_frames, 
               phys_mem.free_frames * PAGE_SIZE / 1024);
        printf("  Orri-taula: 4096 sarrera\n");
    }

    // GARBIKETA
    shared.sim_running = 0;
    shared.done = 1;
    
    pthread_mutex_lock(&shared.mutex);
    shared.scheduler_signal = 1;
    pthread_cond_broadcast(&shared.cond_scheduler);
    pthread_cond_broadcast(&shared.cond);
    pthread_cond_broadcast(&shared.cond2);
    pthread_mutex_unlock(&shared.mutex);
    
    pthread_join(sched_thread, NULL);
    pthread_join(timer_thread_id, NULL);
    pthread_join(clock_tid, NULL);
    
    // Programak askatu
    for (int i = 0; i < 5; i++) {
        if (programs[i]) {
            free_program(programs[i]);
        }
    }
    
    printf("\n INSTRUKZIO bidezko simulazioa ondo amaituta.\n");
    printf(" 5 programa ELF exekutatu dira.\n");
}

// =======================================================
// MAIN FUNTZIO NAGUSIA - KOORDINATZAILEA
// =======================================================

int main() {
    int option;
    
    printf("══════════════════════════════════════════════\n");
    printf("    KERNEL SIMULATZAILEA - KOORDINATZAILEA\n");
    printf("        Sistema Eragileak 2025-2026\n");
    printf("          PROIEKTU OSOA (3 ZATIAK)\n");
    printf("══════════════════════════════════════════════\n");
    printf("Egilea: [ZURE IZENA HEMEN]\n");
    printf("NAN: [ZURE NAN HEMEN]\n");
    printf("Data: %s\n", __DATE__);
    printf("══════════════════════════════════════════════\n");
    
    // Ausazko zenbakiak hasieratu
    srand(time(NULL));
    
    do {
        show_main_menu();
        if (scanf("%d", &option) != 1) {
            while (getchar() != '\n'); // Garbitu bufferra
            printf("\n Sarrera okerra. Zenbaki bat sartu.\n");
            continue;
        }
        
        switch(option) {
            case 1:
                option_1_synchronization();
                break;
            case 2:
                option_2_didactic_menu();
                break;
            case 3:
                option_3_automatic_simulation();
                break;
            case 4:
                option_4_system_info();
                break;
            case 5:
                option_5_memory_virtual_simulation();
                break;
            case 0:
                printf("\nAgur! Kernel Simulatzailea erabiltzeagatik eskerrik asko.\n");
                printf("Proiektua osorik inplementatuta:\n");
                printf("  1. Arkitektura (Clock/Timer, CPU sistema)\n");
                printf("  2. Planifikatzailea (Scheduler, politika aurreratuak)\n");
                printf("  3. Memoria kudeaketa (Memoria birtuala, MMU, TLB)\n");
                break;
            default:
                printf("\n Aukera okerra. 0 eta 5 artean aukeratu.\n");
        }
        
        if (option != 0) {
            printf("\nSakatu Enter menu nagusira itzultzeko...");
            while (getchar() != '\n'); // Garbitu bufferra
            getchar(); // Enter itxaron
        }
        
    } while (option != 0);
    
    return 0;
}