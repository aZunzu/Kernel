#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "config.h"
#include "scheduler.h"
#include "process_generator.h"
#include "process_queue.h"
#include "cpu.h"

int main() {

    SharedData shared;
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond, NULL);
    pthread_cond_init(&shared.cond2, NULL);
    shared.done = 0;

    process_queue_t ready_queue;
    process_queue_t terminated_queue;
    queue_init(&ready_queue);
    queue_init(&terminated_queue);

    cpu_system_t cpu_sys;
    cpu_system_init(&cpu_sys);

    ProcessGenParams pg_params = {
        .shared = &shared,
        .ready_queue = &ready_queue
    };

    SchedulerParams sched_params = {
        .shared = &shared,
        .ready_queue = &ready_queue,
        .terminated_queue = &terminated_queue,
        .cpu_sys = &cpu_sys,
        .policy = POLICY_RULETA_AVANZATUA
    };

    pthread_t pg_thread, sched_thread;
    pthread_create(&pg_thread, NULL, process_generator, &pg_params);
    pthread_create(&sched_thread, NULL, scheduler, &sched_params);

    /* Simulazioa */
    for (int i = 0; i < 25; i++) {
        pthread_mutex_lock(&shared.mutex);
        pthread_cond_signal(&shared.cond);
        pthread_cond_signal(&shared.cond2);
        pthread_mutex_unlock(&shared.mutex);
        sleep(1);
    }

    shared.done = 1;
    pthread_join(pg_thread, NULL);
    pthread_join(sched_thread, NULL);

    printf("Simulazioa amaituta\n");
    return 0;
}
