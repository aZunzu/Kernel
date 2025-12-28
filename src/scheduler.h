#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "config.h"
#include "process_queue.h"
#include "cpu.h"

/* Scheduler-aren parametroak */
typedef struct {
    SharedData* shared;             // Datu partekatuak
    process_queue_t* ready_queue;   // READY egoerako prozesuen ilara
    cpu_system_t* cpu_sys;          // CPU / core / HW thread sistema
} SchedulerParams;

/* Scheduler hari nagusia */
void* scheduler(void* arg);

#endif
