#ifndef EXECUTION_H
#define EXECUTION_H

#include "hardware.h"
#include "pcb.h"

//Exekuzio-motorraren funtzioak 
int execute_step(hw_thread_t* hw, pcb_t* process);
void print_registers(hw_thread_t* hw);
void print_memory_range(uint32_t start, uint32_t end);

#endif