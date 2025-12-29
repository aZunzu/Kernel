#include <stdlib.h>
#include "pcb.h"

/* PCB berri baten hasieraketa */
pcb_t* pcb_create(int pid, int priority) {

    pcb_t* p = malloc(sizeof(pcb_t));
    if (!p) return NULL;

    p->pid = pid;
    p->priority = priority;
    p->exec_time = 10 + rand() % 20;
    p->time_in_cpu = 0;
    p->waiting_time = 0;
    p->state = NEW;
    p->next = NULL;

    return p;
}
