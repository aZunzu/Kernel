#ifndef MEMORY_H
#define MEMORY_H

#include "defines.h"
#include <stdint.h>

// Orri-taula sarrera (Page Table Entry) 
typedef struct {
    uint32_t frame_number : 12;        // Frame fisikoaren zenbakia
    uint8_t present : 1;               // Orria memorian dagoen
    uint8_t read : 1;                  // Irakurtzeko baimena
    uint8_t write : 1;                 // Idazteko baimena
    uint8_t execute : 1;               // Exekutatzeko baimena
    uint8_t accessed : 1;              // Atzitua izan den
    uint8_t dirty : 1;                 // Aldatua izan den
} pte_t;

// Orri-taula
typedef struct {
    pte_t* entries;                    // Orri-taula sarrerak
    uint32_t num_entries;              // Sarrera kopurua
} page_table_t;

// Memoria fisikoa
typedef struct {
    uint8_t* data;                     // Memoria fisikoaren edukia
    uint8_t* allocated;                // Frame bakoitza erabilita dagoen
    uint32_t size;                     // Memoriaren tamaina (byte)
    uint32_t free_frames;              // Frame libreak
} physical_memory_t;

// Memoria kudeaketa PCB-n 
typedef struct {
    uint32_t ptbr;                     // Orri-taula helbide fisikoa
    uint32_t code_start;               // Kode segmentuaren hasiera (birtuala)
    uint32_t data_start;               // Datu segmentuaren hasiera (birtuala)
    page_table_t* page_table;          // Orri-taula (logikoa)
} mm_info_t;

// Memoria funtzioak 
void physical_memory_init();
void* allocate_frame();
void free_frame(void* frame_address);
page_table_t* create_page_table();
void destroy_page_table(page_table_t* pt);
uint32_t translate_address(page_table_t* pt, uint32_t virtual_addr, int write);
uint32_t translate_address_force(page_table_t* pt, uint32_t virtual_addr, int write, int force_write);

// Memoria fisiko globala 
extern physical_memory_t phys_mem;

#endif