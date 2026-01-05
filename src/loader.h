#ifndef LOADER_H
#define LOADER_H

#include "pcb.h"
#include "defines.h"
#include <stdint.h>

// Programa fitxategiaren edukia 
typedef struct {
    uint32_t code_start;               // .text segmentuaren hasiera
    uint32_t data_start;               // .data segmentuaren hasiera
    
    uint32_t* code;                    // Kodea (aginduak)
    uint32_t code_size;                // Kodearen tamaina (instrukzio kopurua)
    
    uint32_t* data;                    // Datuak
    uint32_t data_size;                // Datuen tamaina (hitzean)
} program_t;

// Loader-en funtzioak 
program_t* load_program_from_file(const char* filename);
void free_program(program_t* prog);
pcb_t* create_process_from_program(int pid, int priority, program_t* prog);

// Prozesuren memoria askatu 
void free_process_memory(pcb_t* proc);

#endif