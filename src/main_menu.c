#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "config.h"
#include "cpu.h"
#include "scheduler.h"
#include "process_queue.h"
#include "pcb.h"

/* =======================================================
 * Inprimatzeko laguntzaile funtzioak (GUI grafikoak)
 * ======================================================= */

void print_ready_queue(process_queue_t* q) {
    printf("\n--- READY QUEUE ---\n");
    if (!q->head) {
        printf(" (hutsik)\n");
        return;
    }
    for (pcb_t* p = q->head; p; p = p->next) {
        printf(
            " PID=%d | exec=%d | in_cpu=%d | wait=%d | prio=%d\n",
            p->pid, p->exec_time, p->time_in_cpu,
            p->waiting_time, p->priority
        );
    }
}

void print_blocked_queue(process_queue_t* q) {
    printf("\n--- BLOCKED QUEUE ---\n");
    if (!q->head) {
        printf(" (hutsik)\n");
        return;
    }
    for (pcb_t* p = q->head; p; p = p->next) {
        printf(
            " PID=%d | exec=%d | in_cpu=%d\n",
            p->pid, p->exec_time, p->time_in_cpu
        );
    }
}

void print_running_queue(cpu_system_t* cpu_sys) {
    printf("\n--- RUNNING PROCESSES ---\n");
    int found = 0;

    for (int c = 0; c < cpu_sys->cpu_kop; c++) {
        for (int i = 0; i < cpu_sys->core_kop; i++) {
            for (int h = 0; h < cpu_sys->hw_thread_kop; h++) {
                hw_thread_t* hw =
                    &cpu_sys->cpus[c].cores[i].hw_threads[h];

                if (hw->current_process) {
                    pcb_t* p = hw->current_process;
                    printf(
                        " PID=%d | CPU=%d CORE=%d HW=%d | %d/%d\n",
                        p->pid, c, i, h,
                        p->time_in_cpu, p->exec_time
                    );
                    found = 1;
                }
            }
        }
    }

    if (!found)
        printf(" (ez dago exekutatzen ari den prozesurik)\n");
}

/* =======================================================
 * MENU NAGUSIA (Erabiltzaile interfazea)
 * ======================================================= */

int main() {
    // Datuak partekatutako egitura hasieratu
    SharedData shared = {0};
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond, NULL);
    pthread_cond_init(&shared.cond2, NULL);

    // CPU sistema hasieratu
    cpu_system_t cpu_sys;
    cpu_system_init(&cpu_sys);

    // Prozesu ilarak sortu
    process_queue_t ready_q, blocked_q, terminated_q;
    queue_init(&ready_q);
    queue_init(&blocked_q);
    queue_init(&terminated_q);

    // Scheduler-aren parametroak konfiguratu
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

    int pid = 1;
    int opt;

    // ============================================
    // MENU NAGUSIAREN BEGIZTA
    // ============================================
    while (1) {
        printf("\n==== MENU DIDAKTIKOA ====\n");
        printf("1. Sortu prozesu berria (NEW -> READY)\n");
        printf("2. Scheduler tick exekutatu\n");
        printf("3. RUNNING -> BLOCKED (I/O eskaera)\n");
        printf("4. BLOCKED -> READY (I/O amaiera)\n");
        printf("5. READY queue erakutsi\n");
        printf("6. BLOCKED queue erakutsi\n");
        printf("7. RUNNING egoera erakutsi\n");
        printf("0. Irten\n> ");
        scanf("%d", &opt);

        if (opt == 0) break;

        if (opt == 1) {
            // Prozesu berria sortu
            pcb_t* p = pcb_create(pid++, rand() % 2);
            p->state = READY;
            queue_push(&ready_q, p);
            printf("[NEW] PID=%d READY-ra gehituta\n", p->pid);
        }

        if (opt == 2) {
            // Scheduler-ari tick bat exekutatzeko esan
            pthread_mutex_lock(&shared.mutex);
            pthread_cond_signal(&shared.cond2);
            pthread_mutex_unlock(&shared.mutex);
        }

        /* RUNNING -> BLOCKED transizioa (I/O eskaera) */
        if (opt == 3) {
            int aurkitua = 0;
            for (int c = 0; c < cpu_sys.cpu_kop; c++) {
                for (int i = 0; i < cpu_sys.core_kop; i++) {
                    for (int h = 0; h < cpu_sys.hw_thread_kop; h++) {
                        hw_thread_t* hw =
                            &cpu_sys.cpus[c].cores[i].hw_threads[h];
                        if (hw->current_process) {
                            pcb_t* p = hw->current_process;
                            hw->current_process = NULL;
                            p->state = BLOCKED;
                            queue_push(&blocked_q, p);
                            printf(
                                "[I/O] PID=%d RUNNING -> BLOCKED\n",
                                p->pid
                            );
                            aurkitua = 1;
                            goto done;
                        }
                    }
                }
            }
            done:
            if (!aurkitua) {
                printf("[I/O] Ez dago RUNNING prozesurik\n");
            }
        }

        /* BLOCKED -> READY transizioa (I/O amaiera) */
        if (opt == 4) {
            pcb_t* p = queue_pop(&blocked_q);
            if (p) {
                p->state = READY;
                queue_push(&ready_q, p);
                printf(
                    "[I/O] PID=%d BLOCKED -> READY\n",
                    p->pid
                );
            } else {
                printf("[I/O] Ez dago BLOCKED prozesurik\n");
            }
        }

        if (opt == 5)
            print_ready_queue(&ready_q);

        if (opt == 6)
            print_blocked_queue(&blocked_q);

        if (opt == 7)
            print_running_queue(&cpu_sys);
    }

    // Programa bukatzeko prestaketa
    shared.done = 1;
    pthread_cond_signal(&shared.cond2);
    pthread_join(sched_thread, NULL);
    
    return 0;
}