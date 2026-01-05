#ifndef CPU_H
#define CPU_H

#include "hardware.h"
#include <pthread.h>

/* Core */
typedef struct {
    int id;
    hw_thread_t* hw_threads;
} core_t;

/* CPU */
typedef struct {
    int id;
    core_t* cores;
} cpu_t;

// Sistema osoko CPU egitura
typedef struct {
    cpu_t* cpus;
    int cpu_kop;
    int core_kop;
    int hw_thread_kop;
    pthread_mutex_t mutex;
} cpu_system_t;

void cpu_system_init(cpu_system_t* sys);

void cpu_system_destroy(cpu_system_t* sys);
#endif
