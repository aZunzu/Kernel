#ifndef PCB_H
#define PCB_H

#include "defines.h"
#include "memory.h"

/* Prozesuen egoerak */
typedef enum {
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} process_state_t;

/* Prozesu motak - Exekuzioaren arabera */
typedef enum {
    PROCESS_TICK_BASED,       // 1-2. zatia: TICK kopuru baten arabera exekutatzen da
    PROCESS_INSTRUCTION_BASED // 3. zatia: INSTRUKZIOEN arabera exekutatzen da
} process_type_t;

/* Process Control Block */
typedef struct pcb {
    int pid;                // Prozesuaren ID-a
    int priority;           // 0 = normala, 1 = premiazkoa
    int exec_time;          // Exekuzio denbora totala (tick edo instrukzio max)
    int time_in_cpu;        // CPU-n egindakoa (tick edo instrukzio kopurua)
    int waiting_time;       // READY egoeran daraman denbora (tick)
    process_state_t state;  // Uneko egoera
    process_type_t type;    // Prozesu mota (tick edo instrukzio bidezkoa)
    struct pcb* next;       // Ilara estekatzeko
    
    /* Memoria birtualerako (3. zatia) */
    mm_info_t* mm_info;     // Memoria informazioa
    uint32_t pc;            // Program Counter (birtuala)
    uint32_t exit_code;     // Irteera kodea
    
} pcb_t;

/* PCB berri bat sortzen du (TICK bidezko prozesua) */
pcb_t* pcb_create(int pid, int priority);

/* PCB bat sortzen du memoria birtualarekin (INSTRUKZIO bidezkoa) */
pcb_t* pcb_create_with_memory(int pid, int priority, mm_info_t* mm_info);

/* PCB bat askatzen du */
void pcb_destroy(pcb_t* pcb);

#endif