#ifndef HARDWARE_H
#define HARDWARE_H

#include "defines.h"
#include <stdint.h>

/* PCB aurredeklarazioa (poinerrak soilik behar dira hemen) */
struct pcb;

/* MMU (Memory Management Unit) */
typedef struct {
    // TLB (Translation Lookaside Buffer)
    struct {
        uint32_t virtual_page : 12;    // Orri birtualaren zenbakia
        uint32_t physical_frame : 12;  // Frame fisikoaren zenbakia
        uint8_t valid : 1;             // Baliozko sarrera
        uint8_t dirty : 1;             // Idatzita izan den
    } tlb[TLB_ENTRIES];
    
    int tlb_index;                     // FIFO ordezkapenerako
    uint32_t ptbr;                     // Page Table Base Register
} mmu_t;

/* CPU Hardware Thread-a */
typedef struct hw_thread {
    int id;
    
    /* Hardware erregistro berriak */
    uint32_t pc;                       // Program Counter
    uint32_t ir;                       // Instruction Register
    uint32_t registers[NUM_REGISTERS]; // Erregistro orokorrak
    
    /* MMU bat hardware thread bakoitzeko */
    mmu_t mmu;
    
    /* Prozesu kargatua */
    struct pcb* current_process;
} hw_thread_t;

/* MMU funtzioak */
void mmu_init(mmu_t* mmu);
uint32_t mmu_translate(mmu_t* mmu, uint32_t virtual_addr, int write, uint8_t* page_table);
void mmu_flush_tlb(mmu_t* mmu);

/* Hardware thread funtzioak */
void hw_thread_init(hw_thread_t* hw, int id);

#endif