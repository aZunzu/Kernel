#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "config.h"
#include "scheduler.h"
#include "process_queue.h"
#include "pcb.h"
#include "cpu.h"

static int next_pid = 1;

int main() {

    /* === Egitura partekatuak === */
    SharedData shared;
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond2, NULL);
    shared.done = 0;

    /* === CPU sistema === */
    cpu_system_t* cpu_sys = malloc(sizeof(cpu_system_t));
    cpu_system_init(cpu_sys);

    /* === Ilara === */
    process_queue_t ready_queue;
    process_queue_t terminated_queue;
    queue_init(&ready_queue);
    queue_init(&terminated_queue);

    /* === Scheduler parametroak === */
    SchedulerParams params = {
        .shared = &shared,
        .ready_queue = &ready_queue,
        .terminated_queue = &terminated_queue,
        .cpu_sys = cpu_sys,
        .policy = POLICY_RULETA_AVANZATUA
    };

    pthread_t sched_thread;
    pthread_create(&sched_thread, NULL, scheduler, &params);

    int option;

    do {
        printf("\n===== MENU DIDAKTIKOA =====\n");
        printf("1. Prozesu berria sortu (normal)\n");
        printf("2. Prozesu berria sortu (premiazkoa)\n");
        printf("3. Scheduler tick bat exekutatu\n");
        printf("4. READY ilara erakutsi\n");
        printf("5. Amaitutako prozesuak erakutsi\n");
        printf("0. Irten\n");
        printf("> ");
        scanf("%d", &option);

        switch (option) {

        case 1: {
            pcb_t* p = pcb_create(next_pid++, 0);
            p->state = READY;
            queue_push(&ready_queue, p);
            printf("[MENU] PID %d (normal) READY\n", p->pid);
            break;
        }

        case 2: {
            pcb_t* p = pcb_create(next_pid++, 1);
            p->state = READY;
            queue_push(&ready_queue, p);
            printf("[MENU] PID %d (premiazkoa) READY\n", p->pid);
            break;
        }

        case 3:
            pthread_mutex_lock(&shared.mutex);
            pthread_cond_signal(&shared.cond2);
            pthread_mutex_unlock(&shared.mutex);
            break;

        case 4: {
            pcb_t* p;
            printf("READY ilara:\n");
            for (p = ready_queue.head; p; p = p->next)
                printf(" PID %d | wait=%d | prio=%d\n",
                       p->pid, p->waiting_time, p->priority);
            break;
        }

        case 5: {
            pcb_t* p;
            printf("TERMINATED ilara:\n");
            for (p = terminated_queue.head; p; p = p->next)
                printf(" PID %d\n", p->pid);
            break;
        }

        case 0:
            shared.done = 1;
            pthread_mutex_lock(&shared.mutex);
            pthread_cond_signal(&shared.cond2);
            pthread_mutex_unlock(&shared.mutex);
            break;

        default:
            printf("Aukera okerra\n");
        }

    } while (option != 0);

    pthread_join(sched_thread, NULL);
    return 0;
}
