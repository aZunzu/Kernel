#ifndef PROCESS_GENERATOR_H
#define PROCESS_GENERATOR_H

#include "config.h"
#include "process_queue.h"

typedef struct {
    SharedData* shared;
    process_queue_t* ready_queue;
    int probability;  // 0-100: probabilidade de sortu prozesu
    int* next_pid;    // PID kontagailu partekatua (kanpotik hasieratuta)
} ProcessGenParams;

void* process_generator(void* arg);

#endif
