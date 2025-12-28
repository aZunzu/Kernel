#ifndef PROCESS_GENERATOR_H
#define PROCESS_GENERATOR_H

#include "config.h"
#include "process_queue.h"

typedef struct {
    SharedData* shared;
    process_queue_t* ready_queue;
} ProcessGenParams;

void* process_generator(void* arg);

#endif
