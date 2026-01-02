#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Memoria fisiko globala */
physical_memory_t phys_mem;

/* Memoria fisikoa hasieratu */
void physical_memory_init() {
    phys_mem.size = PHYS_MEM_SIZE;
    phys_mem.data = (uint8_t*)malloc(phys_mem.size);
    phys_mem.allocated = (uint8_t*)calloc(NUM_FRAMES, sizeof(uint8_t));
    
    if (!phys_mem.data || !phys_mem.allocated) {
        fprintf(stderr, "[MEMORY] Errorea: memoria fisikoa hasieratzean\n");
        exit(1);
    }
    
    // Kernelaren memoria erreserbatu
    for (int i = 0; i < KERNEL_FRAMES; i++) {
        phys_mem.allocated[i] = 1;
    }
    
    phys_mem.free_frames = NUM_FRAMES - KERNEL_FRAMES;
    
    printf("[MEMORY] Memoria fisikoa hasieratuta:\n");
    printf("  - Tamaina: %u byte (%u MB)\n", phys_mem.size, phys_mem.size / (1024*1024));
    printf("  - Frame kopurua: %u\n", NUM_FRAMES);
    printf("  - Frame tamaina: %u byte\n", PAGE_SIZE);
    printf("  - Kernel frame-ak: %u-%u\n", 0, KERNEL_FRAMES-1);
    printf("  - User frame libreak: %u\n", phys_mem.free_frames);
}

/* Frame bat esleitu */
void* allocate_frame() {
    for (uint32_t i = USER_FRAME_START; i < NUM_FRAMES; i++) {
        if (!phys_mem.allocated[i]) {
            phys_mem.allocated[i] = 1;
            phys_mem.free_frames--;
            
            void* frame_addr = phys_mem.data + (i * PAGE_SIZE);
            
            printf("[MEMORY] Frame %u esleitu da: %p\n", i, frame_addr);
            return frame_addr;
        }
    }
    
    fprintf(stderr, "[MEMORY] Errorea: ez dago frame libre gehiagorik\n");
    return NULL;
}

/* Frame bat askatu */
void free_frame(void* frame_address) {
    uint32_t offset = (uint8_t*)frame_address - phys_mem.data;
    uint32_t frame_num = offset / PAGE_SIZE;
    
    if (frame_num >= USER_FRAME_START && frame_num < NUM_FRAMES) {
        phys_mem.allocated[frame_num] = 0;
        phys_mem.free_frames++;
        
        printf("[MEMORY] Frame %u askatu da\n", frame_num);
    } else {
        fprintf(stderr, "[MEMORY] Oharra: frame %u ezin da askatu (kernelarena)\n", frame_num);
    }
}

/* Orri-taula bat sortu */
page_table_t* create_page_table() {
    page_table_t* pt = (page_table_t*)malloc(sizeof(page_table_t));
    if (!pt) return NULL;
    
    // Orri kopurua: 2^(24-12) = 2^12 = 4096
    pt->num_entries = 1 << (VIRTUAL_BITS_DEFAULT - PAGE_SIZE_BITS);
    pt->entries = (pte_t*)calloc(pt->num_entries, sizeof(pte_t));
    
    if (!pt->entries) {
        free(pt);
        return NULL;
    }
    
    printf("[MEMORY] Orri-taula berria sortu: %u sarrera\n", pt->num_entries);
    return pt;
}

/* Orri-taula bat suntsitu */
void destroy_page_table(page_table_t* pt) {
    if (pt) {
        free(pt->entries);
        free(pt);
    }
}

/* Helbide birtuala helbide fisiko bihurtu */
uint32_t translate_address(page_table_t* pt, uint32_t virtual_addr, int write) {
    uint32_t page_num = virtual_addr >> PAGE_SIZE_BITS;
    uint32_t offset = virtual_addr & (PAGE_SIZE - 1);
    
    if (page_num >= pt->num_entries) {
        fprintf(stderr, "[MEMORY] Errorea: orri zenbakia handiegia: %u\n", page_num);
        return 0;
    }
    
    pte_t* entry = &pt->entries[page_num];
    
    if (!entry->present) {
        fprintf(stderr, "[MEMORY] Errorea: orria ez dago memorian: %u\n", page_num);
        return 0;
    }
    
    if (write && !entry->write) {
        fprintf(stderr, "[MEMORY] Errorea: idazteko baimenik ez orrian: %u\n", page_num);
        return 0;
    }
    
    // Accessed eta dirty bit-ak eguneratu
    entry->accessed = 1;
    if (write) entry->dirty = 1;
    
    uint32_t physical_addr = (entry->frame_number << PAGE_SIZE_BITS) | offset;
    return physical_addr;
}