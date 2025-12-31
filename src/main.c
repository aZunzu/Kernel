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

// =======================================================
// MENU NAGUSIA - Koordinatzailerako funtzioak
// =======================================================

void show_main_menu() {
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║      KERNEL SIMULATZAILEA - 2025/2026     ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ 1. Clock/Timer sinkronizazioa probatu     ║\n");
    printf("║ 2. Scheduler Menu Didaktikoa              ║\n");
    printf("║ 3. Simulazio automatikoa (erabilera erreala)║\n");
    printf("║ 4. Sistemaren informazioa                 ║\n");
    printf("║ 0. Irten                                  ║\n");
    printf("╚══════════════════════════════════════════╝\n");
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
        .done = 0,
        .tenp_kop = TENP_KOP,
        .sim_running = 1,
        .sim_tick = 0
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
    timer_params[0].izena = "TIMER RAPIDOA";
    
    timer_params[1].shared = &shared;
    timer_params[1].ticks_nahi = TIMER2_TICKS;
    timer_params[1].id = 2;
    timer_params[1].izena = "TIMER ERDIA";
    
    timer_params[2].shared = &shared;
    timer_params[2].ticks_nahi = TIMER3_TICKS;
    timer_params[2].id = 3;
    timer_params[2].izena = "TIMER MANTSOA";
    
    // Timer guztiak sortu
    for (int i = 0; i < TENP_KOP; i++) {
        pthread_create(&timer_tid[i], NULL, timer_thread, &timer_params[i]);
        usleep(50000);
    }
    
    printf("\n⏰ Timer motak:\n");
    printf("- TIMER RAPIDOA: %d tick (%.2f Hz)\n", TIMER1_TICKS, (double)CLOCK_HZ/TIMER1_TICKS);
    printf("- TIMER ERDIA: %d tick (%.2f Hz)\n", TIMER2_TICKS, (double)CLOCK_HZ/TIMER2_TICKS);
    printf("- TIMER MANTSOA: %d tick (%.2f Hz)\n", TIMER3_TICKS, (double)CLOCK_HZ/TIMER3_TICKS);
    
    printf("\nSistema 15 segundoz exekutatzen...\n");
    printf("Ctrl+C sakatu aurretik amaitzeko.\n\n");
    
    // 15 segundoz exekutatu
    for (int i = 15; i > 0; i--) {
        printf("\rGeratzen diren segundoak: %2d", i);
        fflush(stdout);
        sleep(1);
    }
    
    printf("\n\nProba amaitzen...\n");
    
    // Sistemari gelditzeko esan
    shared.sim_running = 0;
    shared.done = 1;
    pthread_mutex_lock(&shared.mutex);
    pthread_cond_broadcast(&shared.cond);
    pthread_cond_broadcast(&shared.cond2);
    pthread_mutex_unlock(&shared.mutex);
    
    // Hariak itxaron
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

// Inprimatzeko laguntzaile funtzioak
void print_ready_queue_menu(process_queue_t* q) {
    printf("\n--- READY QUEUE ---\n");
    if (!q->head) {
        printf(" (hutsik)\n");
        return;
    }
    for (pcb_t* p = q->head; p; p = p->next) {
        printf(" PID=%d | exec=%d | cpu=%d | wait=%d | prio=%d\n",
               p->pid, p->exec_time, p->time_in_cpu, p->waiting_time, p->priority);
    }
}

void print_blocked_queue_menu(process_queue_t* q) {
    printf("\n--- BLOCKED QUEUE ---\n");
    if (!q->head) {
        printf(" (hutsik)\n");
        return;
    }
    for (pcb_t* p = q->head; p; p = p->next) {
        printf(" PID=%d | exec=%d | cpu=%d\n",
               p->pid, p->exec_time, p->time_in_cpu);
    }
}

void print_running_queue_menu(cpu_system_t* cpu_sys) {
    printf("\n--- RUNNING PROZESUAK ---\n");
    int found = 0;

    for (int c = 0; c < cpu_sys->cpu_kop; c++) {
        for (int i = 0; i < cpu_sys->core_kop; i++) {
            for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                hw_thread_t* hw = &cpu_sys->cpus[c].cores[i].hw_threads[h];
                if (hw->current_process) {
                    pcb_t* p = hw->current_process;
                    printf(" PID=%d | CPU=%d CORE=%d HW=%d | %d/%d\n",
                           p->pid, c, i, h, p->time_in_cpu, p->exec_time);
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
    printf("ikasteko modu interaktiboa.\n");
    printf("---------------------------------------------\n");
    printf("GARRANTZITSUA: '2' sakatu ondoren, itxaron scheduler-aren\n");
    printf("irteera ikusteko. Ondoren, sakatu Enter jarraitzeko.\n\n");
    
    // Datu partekatuak hasieratu
    SharedData shared;
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond, NULL);
    pthread_cond_init(&shared.cond2, NULL);
    shared.done = 0;
    shared.tenp_kop = 0;
    shared.sim_running = 1;
    shared.sim_tick = 0;
    
    // CPU sistema hasieratu
    cpu_system_t cpu_sys;
    cpu_system_init(&cpu_sys);
    
    // Prozesu ilarak sortu
    process_queue_t ready_q, blocked_q, terminated_q;
    queue_init(&ready_q);
    queue_init(&blocked_q);
    queue_init(&terminated_q);
    
    // Scheduler parametroak konfiguratu
    SchedulerParams sched = {
        .shared = &shared,
        .ready_queue = &ready_q,
        .blocked_queue = &blocked_q,
        .terminated_queue = &terminated_q,
        .cpu_sys = &cpu_sys,
        .policy = POLICY_RULETA_AVANZATUA
    };
    
    // Scheduler hari bat sortu
    pthread_t sched_thread;
    pthread_create(&sched_thread, NULL, scheduler, &sched);
    
    // Scheduler hasieratu arte itxaron
    usleep(200000);
    
    int pid = 1;
    int opt;
    int tick_count = 0;
    
    // MENU NAGUSIAREN BEGIZTA
    while (1) {
        printf("\n══════════════════════════════════════════════\n");
        printf("MENU DIDAKTIKOA - TICK: %d\n", tick_count);
        printf("══════════════════════════════════════════════\n");
        printf("1. Sortu prozesu berria (NEW -> READY)\n");
        printf("2. Exekutatu scheduler TICK bat\n");
        printf("3. RUNNING -> BLOCKED (I/O eskaera)\n");
        printf("4. BLOCKED -> READY (I/O amaiera)\n");
        printf("5. Erakutsi prozesu ilarak\n");
        printf("6. Erakutsi CPU egoera\n");
        printf("0. Menu nagusira itzuli\n");
        printf("══════════════════════════════════════════════\n");
        printf("Aukeratu: ");
        
        if (scanf("%d", &opt) != 1) {
            while (getchar() != '\n');
            printf("\n❌ Sarrera okerra. Zenbaki bat sartu.\n");
            continue;
        }
        
        if (opt == 0) break;
        
        switch(opt) {
            case 1: {
                pcb_t* p = pcb_create(pid++, rand() % 2);
                p->state = READY;
                p->exec_time = 5 + rand() % 15;  // Exekuzio denbora laburragoa
                queue_push(&ready_q, p);
                printf("\n✅ Prozesu berria: PID=%d (Exec=%d, Prio=%d)\n", 
                       p->pid, p->exec_time, p->priority);
                break;
            }
            
            case 2: {
                tick_count++;
                printf("\n══════════════════════════════════════════════\n");
                printf("EXEKUTATZEN: TICK #%d\n", tick_count);
                printf("══════════════════════════════════════════════\n");
                
                // Scheduler tick bat exekutatzeko
                pthread_mutex_lock(&shared.mutex);
                pthread_cond_signal(&shared.cond2);
                pthread_mutex_unlock(&shared.mutex);
                
                // Itxaron scheduler-aren irteera ikusteko
                usleep(500000);  // 0.5 segundo
                
                // Erakutsi mezu bat eta itxaron Enter
                printf("\n══════════════════════════════════════════════\n");
                printf("TICK #%d bukatu da. Sakatu Enter jarraitzeko...", tick_count);
                while (getchar() != '\n'); // Garbitu bufferra
                getchar(); // Enter itxaron
                break;
            }
            
            case 3: {
                // RUNNING -> BLOCKED transizioa (I/O eskaera)
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
                                printf("\n⏸️  I/O eskaera: PID=%d RUNNING -> BLOCKED\n", p->pid);
                                aurkitua = 1;
                                goto io_done;
                            }
                        }
                    }
                }
                io_done:
                if (!aurkitua) {
                    printf("\n⚠️  Ez dago RUNNING prozesurik\n");
                }
                break;
            }
            
            case 4: {
                // BLOCKED -> READY transizioa (I/O amaiera)
                pcb_t* p = queue_pop(&blocked_q);
                if (p) {
                    p->state = READY;
                    queue_push(&ready_q, p);
                    printf("\n🔄 I/O amaiera: PID=%d BLOCKED -> READY\n", p->pid);
                } else {
                    printf("\n⚠️  Ez dago BLOCKED prozesurik\n");
                }
                break;
            }
            
            case 5: {
                printf("\n=== PROZESU ILARAK ===\n");
                print_ready_queue_menu(&ready_q);
                print_blocked_queue_menu(&blocked_q);
                
                // Terminated queue erakutsi
                printf("\n--- TERMINATED QUEUE ---\n");
                if (!terminated_q.head) {
                    printf(" (hutsik)\n");
                } else {
                    for (pcb_t* p = terminated_q.head; p; p = p->next) {
                        printf(" PID=%d | exec=%d | cpu=%d\n",
                               p->pid, p->exec_time, p->time_in_cpu);
                    }
                }
                break;
            }
            
            case 6: {
                print_running_queue_menu(&cpu_sys);
                break;
            }
                
            default:
                printf("\n❌ Aukera okerra. 0 eta 6 artean aukeratu.\n");
        }
    }
    
    // Programa bukatzeko
    shared.done = 1;
    shared.sim_running = 0;
    pthread_mutex_lock(&shared.mutex);
    pthread_cond_signal(&shared.cond2);
    pthread_mutex_unlock(&shared.mutex);
    
    printf("\nScheduler hariaren amaiera itxaroten...\n");
    pthread_join(sched_thread, NULL);
    
    // Mutex eta condition variable-ak suntsitu
    pthread_mutex_destroy(&shared.mutex);
    pthread_cond_destroy(&shared.cond);
    pthread_cond_destroy(&shared.cond2);
    
    printf("\n✅ Menu didaktikoa amaituta.\n");
}

// =======================================================
// 3. AUKERA: Simulazio automatikoa (erabilera erreala)
// =======================================================

void option_3_automatic_simulation() {
    printf("\n=== 3. AUKERA: SIMULAZIO AUTOMATIKOA ===\n");
    printf("Kernel baten funtzionamendua modu errealistan\n");
    printf("simulatzen du, ataza guztiak integratuta.\n");
    printf("--------------------------------------------\n\n");
    
    printf("Sistemaren konfigurazioa:\n");
    printf("- Clock: %.1f Hz\n", CLOCK_HZ);
    printf("- Timerrak: %d\n", TENP_KOP);
    printf("- Politika: Ruleta Aurreratua\n");
    printf("- CPU: %d, Core: %d, HW Thread: %d\n", 
           CPU_KOP, CORE_KOP, HW_THREAD_KOP);
    printf("\nSimulazioa abiarazten...\n\n");
    
    // 1. INICIALIZAR SISTEMA
    SharedData shared;
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond, NULL);
    pthread_cond_init(&shared.cond2, NULL);
    shared.done = 0;
    shared.tenp_kop = 0;
    shared.sim_running = 1;
    shared.sim_tick = 0;
    
    cpu_system_t cpu_sys;
    cpu_system_init(&cpu_sys);
    
    process_queue_t ready_q, blocked_q, terminated_q;
    queue_init(&ready_q);
    queue_init(&blocked_q);
    queue_init(&terminated_q);
    
    // 2. SORTU PROZESU HASIERAKOAK
    printf("Prozesu hasierakoak sortzen...\n");
    for (int i = 0; i < 5; i++) {
        pcb_t* p = pcb_create(i+1, rand() % 2);
        p->state = READY;
        p->exec_time = 5 + rand() % 15;
        queue_push(&ready_q, p);
        printf("  PID=%d sortuta (Prio=%d, Exec=%d)\n", 
               p->pid, p->priority, p->exec_time);
    }
    
    // 3. KONFIGURATU SCHEDULER
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
    
    // Scheduler hasieratu arte itxaron
    usleep(200000);
    
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║      SIMULAZIOA MARTXAN                  ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Iraupena: 15 tick                        ║\n");
    printf("║ Egoera: Exekutatzen...                   ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");
    
    // 4. SIMULAZIO BEGIZTA NAGUSIA
    int tick_max = 15;
    char input;
    
    for (int tick = 1; tick <= tick_max && shared.sim_running; tick++) {
        shared.sim_tick = tick;
        
        // Erakutsi tick-a
        printf("\n══════════════════════════════════════════════\n");
        printf(" TICK #%d - AUTOMATIKOA\n", tick);
        printf("══════════════════════════════════════════════\n");
        
        // Exekutatu scheduler tick bat
        pthread_mutex_lock(&shared.mutex);
        pthread_cond_signal(&shared.cond2);
        pthread_mutex_unlock(&shared.mutex);
        
        // Itxaron scheduler-aren irteera ikusteko
        usleep(800000);  // 0.8 segundo
        
        // EKINTZA ALEATORIOAK SIMULATU (zuzenean ilarak erabiliz)
        
        // 1. Prozesu berria sortu (30% probabilitatea)
        if (rand() % 100 < 30) {
            pcb_t* p = pcb_create(100 + tick, rand() % 2);
            p->state = READY;
            p->exec_time = 3 + rand() % 10;
            queue_push(&ready_q, p);
            printf("\n[EKINTZA] ✨ Prozesu berria: PID=%d (Exec=%d)\n", p->pid, p->exec_time);
        }
        
        // 2. I/O eskaera (20% probabilitatea)
        if (rand() % 100 < 20) {
            // Bilatu RUNNING prozesu bat
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
                            printf("\n[EKINTZA] ⏸️  I/O eskaera: PID=%d BLOCKED-era\n", p->pid);
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
        
        // 3. I/O amaiera (30% probabilitatea)
        if (rand() % 100 < 30 && blocked_q.head) {
            pcb_t* p = queue_pop(&blocked_q);
            if (p) {
                p->state = READY;
                queue_push(&ready_q, p);
                printf("\n[EKINTZA] 🔄 I/O amaiera: PID=%d READY-ra\n", p->pid);
            }
        }
        
        // Erakutsi egoera laburra
        printf("\n[EGOERA LABURRA]\n");
        
        // RUNNING kontatu
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
        
        // Pausa txiki bat
        usleep(500000);
    }
    
    // 5. SIMULAZIOA AMAITU
    printf("\n══════════════════════════════════════════════\n");
    printf(" SIMULAZIOA AMAITUTA\n");
    printf("══════════════════════════════════════════════\n\n");
    
    // Emaitzak erakutsi
    printf("🏁 FINAL EGOERA:\n");
    printf("  Tick guztiak: %d\n", shared.sim_tick);
    printf("  Prozesu totalak: %d\n", 
           queue_count(&ready_q) + queue_count(&blocked_q) + queue_count(&terminated_q));
    printf("  Prozesu bukatuak: %d\n", queue_count(&terminated_q));
    printf("  READY egoeran: %d\n", queue_count(&ready_q));
    printf("  BLOCKED egoeran: %d\n", queue_count(&blocked_q));
    
    // Bukaera tasa kalkulatu
    int total_procesos = queue_count(&ready_q) + queue_count(&blocked_q) + queue_count(&terminated_q);
    if (total_procesos > 0) {
        printf("  Bukaera tasa: %.1f%%\n", 
               (queue_count(&terminated_q) * 100.0) / total_procesos);
    }
    
    // 6. GARBIKETA
    shared.sim_running = 0;
    shared.done = 1;
    pthread_mutex_lock(&shared.mutex);
    pthread_cond_signal(&shared.cond2);
    pthread_mutex_unlock(&shared.mutex);
    
    pthread_join(sched_thread, NULL);
    
    printf("\n✅ Simulazio automatikoa ondo amaituta.\n");
}

// =======================================================
// 4. AUKERA: Sistemaren informazioa
// =======================================================

void option_4_system_info() {
    printf("\n=== SISTEMAREN INFORMAZIOA ===\n");
    printf("Kernel Simulatzailea - Sistema Eragileak 2025/2026\n");
    printf("\nOsagai inplementatuak:\n");
    printf("✅ Clock (sistemaren erlojua) - %.1f Hz\n", CLOCK_HZ);
    printf("✅ Timer anitzak frekuntza desberdinetan (%d)\n", TENP_KOP);
    printf("✅ Prozesuen sortzailea (Process Generator)\n");
    printf("✅ Scheduler (Ruleta Aurreratua politika)\n");
    printf("✅ CPU sistema multi-core (%d CPU × %d Core × %d HW Thread)\n",
           CPU_KOP, CORE_KOP, HW_THREAD_KOP);
    printf("✅ Prozesuen ilarak (Ready, Blocked, Terminated)\n");
    printf("✅ PCB (Process Control Block)\n");
    printf("\nProiektuaren zatiak:\n");
    printf("1. Sistemaren arkitektura - Oinarrizko egitura eta sinkronizazioa\n");
    printf("2. Planifikatzailea - Prozesuen exekuzioaren kudeaketa\n");
    printf("3. Memoriaren kudeatzailea - (ETORKIZUNEAN IMPLEMENTATZEKOA)\n");
    printf("\nEragiketa moduak:\n");
    printf("1. Sinkronizazio proba (Clock/Timer)\n");
    printf("2. Menu didaktikoa (Scheduler interaktiboa)\n");
    printf("3. Simulazio automatikoa (erabilera erreala)\n");
    printf("\nGarapen egoera:\n");
    printf("- Parte 1 (Arkitektura): ✅ Osatuta\n");
    printf("- Parte 2 (Scheduler): ✅ Osatuta\n");
    printf("- Parte 3 (Memoria): 🔄 Garapen fasean\n");
}

// =======================================================
// MAIN FUNTZIO NAGUSIA - KOORDINATZAILEA
// =======================================================

int main() {
    int option;
    
    printf("══════════════════════════════════════════════\n");
    printf("    KERNEL SIMULATZAILEA - KOORDINATZAILEA\n");
    printf("        Sistema Eragileak 2025-2026\n");
    printf("══════════════════════════════════════════════\n");
    
    // Ausazko zenbakiak hasieratu
    srand(time(NULL));
    
    do {
        show_main_menu();
        if (scanf("%d", &option) != 1) {
            while (getchar() != '\n'); // Garbitu bufferra
            printf("\n❌ Sarrera okerra. Zenbaki bat sartu.\n");
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
            case 0:
                printf("\nAgur! Simulatzailea erabiltzeagatik eskerrik asko.\n");
                break;
            default:
                printf("\n❌ Aukera okerra. 0 eta 4 artean aukeratu.\n");
        }
        
        if (option != 0) {
            printf("\nSakatu Enter menu nagusira itzultzeko...");
            while (getchar() != '\n'); // Garbitu bufferra
            getchar(); // Enter itxaron
        }
        
    } while (option != 0);
    
    return 0;
}