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

// Funtzio laguntzaileak inprimitzeko
const char* get_process_type_name(process_type_t type) {
    switch(type) {
        case PROCESS_TICK_BASED: return "TICK bidezkoa";
        case PROCESS_INSTRUCTION_BASED: return "INSTRUKZIO bidezkoa";
        default: return "EZEZAGUNA";
    }
}

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
    
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║      SIMULAZIOA MARTXAN                  ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Iraupena: 20 tick                        ║\n");
    printf("║ Prozesu mota: TICK bidezkoak bakarrik   ║\n");
    printf("║ Scheduler periodoa: %d tick             ║\n", timer_params.ticks_nahi);
    printf("╚══════════════════════════════════════════╝\n\n");
    
    int tick_max = 20;
    
    for (int tick = 1; tick <= tick_max && shared.sim_running; tick++) {
        shared.sim_tick = tick;
        
        printf("\n══════════════════════════════════════════════\n");
        printf(" TICK #%d - SIMULAZIO AUTOMATIKOA (TICK bidezkoak)\n", tick);
        printf("══════════════════════════════════════════════\n");
        
        // EKINTZA ALEATORIOAK
        if (rand() % 100 < 25) {
            pcb_t* p = pcb_create(100 + tick, rand() % 2);
            p->state = READY;
            p->type = PROCESS_TICK_BASED;
            p->exec_time = 2 + rand() % 6;
            queue_push(&ready_q, p);
            printf("\n[EKINTZA] TICK bidezko prozesu berria: PID=%d (Exec=%d TICK)\n", 
                   p->pid, p->exec_time);
        }
        
        if (rand() % 100 < 20) {
            int found = 0;
            for (int c = 0; c < cpu_sys.cpu_kop; c++) {
                for (int i = 0; i < cpu_sys.core_kop; i++) {
                    for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                        hw_thread_t* hw = &cpu_sys.cpus[c].cores[i].hw_threads[h];
                        if (hw->current_process) {
                            pcb_t* p = hw->current_process;
                            hw->current_process = NULL;
                            p->state = BLOCKED;
                            queue_push(&blocked_q, p);
                            printf("\n[EKINTZA] I/O eskaera: PID=%d (TICK bidezkoa) → BLOCKED\n", p->pid);
                            found = 1;
                            goto io_action_done;
                        }
                    }
                }
            }
            io_action_done:
            if (!found && tick > 3) {
                printf("\n[EKINTZA] Ez dago RUNNING prozesurik I/O eskaerarako\n");
            }
        }
        
        if (rand() % 100 < 30 && blocked_q.head) {
            pcb_t* p = queue_pop(&blocked_q);
            if (p) {
                p->state = READY;
                queue_push(&ready_q, p);
                printf("\n[EKINTZA] I/O amaiera: PID=%d → READY\n", p->pid);
            }
        }
        
        printf("\n[EGOERA LABURRA]\n");
        
        int running_count = 0;
        for (int c = 0; c < cpu_sys.cpu_kop; c++) {
            for (int i = 0; i < cpu_sys.core_kop; i++) {
                for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                    if (cpu_sys.cpus[c].cores[i].hw_threads[h].current_process) {
                        running_count++;
                    }
                }
            }
        }
        
        printf("  RUNNING: %d | READY: %d | BLOCKED: %d | TERMINATED: %d\n",
               running_count, 
               queue_count(&ready_q),
               queue_count(&blocked_q),
               queue_count(&terminated_q));
        
        usleep(300000);
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
    printf("2. Programa fitxategiak sortzen (simulazioa)...\n");
    printf("   Oharra: Benetako prometheus.c erabiliz programa errealak sortu\n");
    printf("   Adibidez: ./prometheus -s 0 -nprog -f0 -l5 -p3\n\n");
    
    printf("   Prog000.elf fitxategiaren edukia (simulatua):\n");
    printf("   .text 000000\n");
    printf("   .data 000020\n");
    printf("   0F000000  // ld R15, [0x000000]\n");
    printf("   01000004  // ld R1, [0x000004]\n");
    printf("   22010000  // add R2, R1, R0\n");
    printf("   12000008  // st R2, [0x000008]\n");
    printf("   F0000000  // exit\n");
    printf("   0000000A  // data: 10\n");
    printf("   00000014  // data: 20\n");
    printf("   00000000  // data: 0 (result gordetzeko)\n");
    
    printf("\nFAZE 2: Programen exekuzioa\n");
    printf("----------------------------\n");
    
    // 3. Programa bat kargatu (simulatuta)
    printf("3. Programa bat kargatzen (simulatuta)...\n");
    
    // Simulatu programa bat
    program_t* prog = (program_t*)malloc(sizeof(program_t));
    prog->code_start = 0x000000;
    prog->data_start = 0x000020;
    prog->code_size = 5;  // 4 instrukzio + exit
    prog->data_size = 3;  // 3 datu hitz
    
    // Kodea (simulatua)
    prog->code = (uint32_t*)malloc(prog->code_size * sizeof(uint32_t));
    prog->code[0] = 0x0F000000;  // ld R15, [0x000000]
    prog->code[1] = 0x01000004;  // ld R1, [0x000004]
    prog->code[2] = 0x22010000;  // add R2, R1, R0
    prog->code[3] = 0x12000008;  // st R2, [0x000008]
    prog->code[4] = 0xF0000000;  // exit
    
    // Datuak (simulatuta)
    prog->data = (uint32_t*)malloc(prog->data_size * sizeof(uint32_t));
    prog->data[0] = 0x0000000A;  // 10
    prog->data[1] = 0x00000014;  // 20
    prog->data[2] = 0x00000000;  // 0 (result)
    
    printf("   Programa kargatu: 5 instrukzio, 3 datu\n");
    
    // 4. Prozesua sortu programatik
    printf("4. INSTRUKZIO bidezko prozesua sortzen programatik...\n");
    pcb_t* process = create_process_from_program(1, 0, prog);
    if (!process) {
        printf("Errorea: ezin izan da prozesua sortu\n");
        free(prog->code);
        free(prog->data);
        free(prog);
        return;
    }
    
    process->type = PROCESS_INSTRUCTION_BASED;
    process->state = READY;
    process->exec_time = 10;  // Max 10 instrukzio
    
    printf("   Prozesua sortuta: PID=%d, Mota: %s, PC=0x%06X\n", 
           process->pid, get_process_type_name(process->type), process->pc);
    
    // 5. CPU sistema hasieratu
    printf("5. CPU sistema hasieratzen (hardware berria)...\n");
    cpu_system_t cpu_sys;
    cpu_system_init(&cpu_sys);
    
    // 6. Prozesu ilarak sortu
    process_queue_t ready_q, blocked_q, terminated_q;
    queue_init(&ready_q);
    queue_init(&blocked_q);
    queue_init(&terminated_q);
    
    queue_push(&ready_q, process);
    
    // 7. Beste prozesu batzuk sortu (hibridoa: TICK + INSTRUKZIO)
    printf("6. Prozesu gehiago sortzen (sistema hibridoa)...\n");
    
    // TICK bidezko prozesu bat
    pcb_t* tick_proc = pcb_create(2, 0);
    tick_proc->type = PROCESS_TICK_BASED;
    tick_proc->state = READY;
    tick_proc->exec_time = 8;
    queue_push(&ready_q, tick_proc);
    printf("   PID=%d: TICK bidezko prozesua (Exec=%d TICK)\n", 
           tick_proc->pid, tick_proc->exec_time);
    
    // INSTRUKZIO bidezko beste prozesu bat (programa berdinarekin, bere orri-taularekin)
    pcb_t* instr_proc2 = create_process_from_program(3, 1, prog);
    if (instr_proc2) {
        instr_proc2->type = PROCESS_INSTRUCTION_BASED;
        instr_proc2->state = READY;
        instr_proc2->exec_time = 7;
        instr_proc2->pc = prog->code_start;
        queue_push(&ready_q, instr_proc2);
        printf("   PID=%d: INSTRUKZIO bidezko prozesua (Prio=1, Instrukzio max=%d)\n", 
               instr_proc2->pid, instr_proc2->exec_time);
    } else {
        printf("   PID=3 sortzean errorea (INSTRUKZIO)\n");
    }
    
    printf("\nFAZE 3: Exekuzioa\n");
    printf("------------------\n");

    // Debug helbideak: data segmentua hasieran
    uint32_t dbg_data_vstart = prog->data_start;
    uint32_t dbg_data_vend = prog->data_start + (prog->data_size * sizeof(uint32_t)) - 1;
    uint32_t dbg_data_pstart = translate_address(process->mm_info->page_table, dbg_data_vstart, 0);
    uint32_t dbg_data_pend = translate_address(process->mm_info->page_table, dbg_data_vend, 0);
    if (dbg_data_pstart && dbg_data_pend) {
        printf("\n[DEBUG] Data segmentua (fisikoa) HASIERA:\n");
        print_memory_range(dbg_data_pstart, dbg_data_pend);
    }
    
    // 8. Scheduler konfiguratu
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
    
    // 9. Clock eta Timer sortu
    ClockParams clock_params = {&shared, CLOCK_HZ};
    pthread_t clock_tid;
    pthread_create(&clock_tid, NULL, clock_thread, &clock_params);
    
    TimerParams timer_params;
    timer_params.shared = &shared;
    timer_params.ticks_nahi = 1;  // Tick bakoitzean scheduler aktibatu
    timer_params.id = 1;
    timer_params.izena = "MEMORIA TIMER";
    timer_params.activate_scheduler = 1;
    
    pthread_t timer_thread_id;
    pthread_create(&timer_thread_id, NULL, timer_thread, &timer_params);
    
    usleep(500000);
    
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║      MEMORIA BIRTUALAREN SIMULAZIOA         ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Prozesu motak: TICK + INSTRUKZIO bidezkoak  ║\n");
    printf("║ Prozesu kopurua: 3 (1 TICK, 2 INSTRUKZIO)   ║\n");
    printf("║ Memoria: %u frame libre                   ║\n", phys_mem.free_frames);
    printf("║ Scheduler periodoa: 1 tick                 ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    
    // 10. SIMULAZIO BEGIZTA
    int tick_max = 25;
    
    for (int tick = 1; tick <= tick_max && shared.sim_running; tick++) {
        shared.sim_tick = tick;
        
        printf("\n══════════════════════════════════════════════\n");
        printf(" TICK #%d - MEMORIA BIRTUALA (INSTRUKZIO bidezkoak)\n", tick);
        printf("══════════════════════════════════════════════\n");
        
        // EKINTZA ALEATORIOAK (I/O simulatua)
        if (rand() % 100 < 15) {
            int found = 0;
            for (int c = 0; c < cpu_sys.cpu_kop; c++) {
                for (int i = 0; i < cpu_sys.core_kop; i++) {
                    for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                        hw_thread_t* hw = &cpu_sys.cpus[c].cores[i].hw_threads[h];
                        if (hw->current_process) {
                            pcb_t* p = hw->current_process;
                            hw->current_process = NULL;
                            p->state = BLOCKED;
                            queue_push(&blocked_q, p);
                            printf("\n[EKINTZA] I/O eskaera: PID=%d (%s) → BLOCKED\n", 
                                   p->pid, get_process_type_name(p->type));
                            found = 1;
                            goto mem_io_done;
                        }
                    }
                }
            }
            mem_io_done:
            if (!found && tick > 5) {
                printf("\n[EKINTZA] Ez dago RUNNING prozesurik\n");
            }
        }
        
        if (rand() % 100 < 25 && blocked_q.head) {
            pcb_t* p = queue_pop(&blocked_q);
            if (p) {
                p->state = READY;
                queue_push(&ready_q, p);
                printf("\n[EKINTZA] I/O amaiera: PID=%d (%s) → READY\n", 
                       p->pid, get_process_type_name(p->type));
            }
        }
        
        // EGOERA LABURRA
        printf("\n[EGOERA LABURRA]\n");
        
        int running_count = 0;
        int tick_running = 0;
        int instruction_running = 0;
        
        for (int c = 0; c < cpu_sys.cpu_kop; c++) {
            for (int i = 0; i < cpu_sys.core_kop; i++) {
                for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                    pcb_t* p = cpu_sys.cpus[c].cores[i].hw_threads[h].current_process;
                    if (p) {
                        running_count++;
                        if (p->type == PROCESS_TICK_BASED) tick_running++;
                        else instruction_running++;
                    }
                }
            }
        }
        
        printf("  RUNNING: %d (TICK:%d, INSTR:%d) | READY: %d | BLOCKED: %d | TERMINATED: %d\n",
               running_count, tick_running, instruction_running,
               queue_count(&ready_q),
               queue_count(&blocked_q),
               queue_count(&terminated_q));
        
        if (tick % 5 == 0 && phys_mem.data != NULL) {
            printf("  Memoria: %u frame libre (%u KB)\n",
                   phys_mem.free_frames, 
                   phys_mem.free_frames * PAGE_SIZE / 1024);
        }
        
        usleep(350000);
        
        // Amaiera baldintzak
        if (queue_count(&terminated_q) >= 2) {
            printf("\n[OHARRA] 2 prozesu baino gehiago bukatu dira. Simulazioa amaitzen...\n");
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
    }
    
    // Debug: erakutsi data segmentua exekuzioa amaitu ondoren (fisikoki)
    dbg_data_pstart = translate_address(process->mm_info->page_table, dbg_data_vstart, 0);
    dbg_data_pend = translate_address(process->mm_info->page_table, dbg_data_vend, 0);
    if (dbg_data_pstart && dbg_data_pend) {
        printf("\n[DEBUG] Data segmentua (fisikoa) AMAIERA:\n");
        print_memory_range(dbg_data_pstart, dbg_data_pend);
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
    
    // Programa askatu
    free(prog->code);
    free(prog->data);
    free(prog);
    
    printf("\n INSTRUKZIO bidezko simulazioa ondo amaituta.\n");
    printf(" 3. zatia (Memoria kudeaketa) osorik inplementatuta.\n");
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