#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "config.h"
#include "process_queue.h"
#include "cpu.h"

// Planifikazio politikak
typedef enum {
    POLICY_FIFO,
    POLICY_RULETA_AVANZATUA
} sched_policy_t;

// Scheduler-aren parametroak
typedef struct {
    SharedData* shared;
    process_queue_t* ready_queue;
    process_queue_t* blocked_queue;
    process_queue_t* terminated_queue;
    cpu_system_t* cpu_sys;
    sched_policy_t policy;
    int simulation_mode;  // 3=TICK, 5=INSTRUKZIO
} SchedulerParams;

// Scheduler haria
void* scheduler(void* arg);

// Politika nagusia
pcb_t* select_next_process(process_queue_t* q, sched_policy_t policy);

#endif
