#include "loader.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Programa bat kargatu fitxategitik */
program_t* load_program_from_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "[LOADER] Errorea: ezin izan da fitxategia ireki: %s\n", filename);
        return NULL;
    }
    
    program_t* prog = (program_t*)malloc(sizeof(program_t));
    if (!prog) {
        fclose(file);
        return NULL;
    }
    
    char line[256];
    
    // 1. .text eta .data irakurri
    if (fgets(line, sizeof(line), file)) {
        sscanf(line, ".text %x", &prog->code_start);
    }
    
    if (fgets(line, sizeof(line), file)) {
        sscanf(line, ".data %x", &prog->data_start);
    }
    
    // 2. Kodea irakurri (exit arte)
    prog->code = NULL;
    prog->code_size = 0;
    
    while (fgets(line, sizeof(line), file)) {
        uint32_t instruction;
        if (sscanf(line, "%x", &instruction) != 1) {
            break;
        }
        
        // Exit agindua?
        if ((instruction >> 28) == OPCODE_EXIT) {
            // Gehitu exit agindua eta irten
            prog->code = realloc(prog->code, (prog->code_size + 1) * sizeof(uint32_t));
            prog->code[prog->code_size++] = instruction;
            break;
        }
        
        // Beste agindua gehitu
        prog->code = realloc(prog->code, (prog->code_size + 1) * sizeof(uint32_t));
        prog->code[prog->code_size++] = instruction;
    }
    
    // 3. Datuak irakurri
    prog->data = NULL;
    prog->data_size = 0;
    
    while (fgets(line, sizeof(line), file)) {
        uint32_t value;
        if (sscanf(line, "%x", &value) != 1) {
            break;
        }
        
        prog->data = realloc(prog->data, (prog->data_size + 1) * sizeof(uint32_t));
        prog->data[prog->data_size++] = value;
    }
    
    fclose(file);
    
    printf("[LOADER] Programa kargatu: %s\n", filename);
    printf("  - Code: %u instrukzio (0x%06X - 0x%06X)\n", 
           prog->code_size, prog->code_start, 
           prog->code_start + prog->code_size * 4 - 1);
    printf("  - Data: %u hitz (0x%06X - 0x%06X)\n", 
           prog->data_size, prog->data_start,
           prog->data_start + prog->data_size * 4 - 1);
    
    return prog;
}

/* Programa bat askatu */
void free_program(program_t* prog) {
    if (prog) {
        free(prog->code);
        free(prog->data);
        free(prog);
    }
}

/* Prozesu bat sortu programatik */
pcb_t* create_process_from_program(int pid, int priority, program_t* prog) {
    pcb_t* pcb = pcb_create(pid, priority);
    if (!pcb) return NULL;
    
    // Memoria informazioa hasieratu
    pcb->mm_info = (mm_info_t*)malloc(sizeof(mm_info_t));
    if (!pcb->mm_info) {
        free(pcb);
        return NULL;
    }
    
    // 1. Orri-taula sortu
    pcb->mm_info->page_table = create_page_table();
    if (!pcb->mm_info->page_table) {
        free(pcb->mm_info);
        free(pcb);
        return NULL;
    }
    
    // 2. Kode segmentua kargatu memorian
    pcb->mm_info->code_start = prog->code_start;
    for (uint32_t i = 0; i < prog->code_size; i++) {
        uint32_t vaddr = prog->code_start + (i * 4);
        uint32_t page_num = vaddr >> PAGE_SIZE_BITS;
        
        // Orri bat sortu behar bada
        if (!pcb->mm_info->page_table->entries[page_num].present) {
            void* frame = allocate_frame();
            if (!frame) {
                destroy_page_table(pcb->mm_info->page_table);
                free(pcb->mm_info);
                free(pcb);
                return NULL;
            }
            
            pte_t* entry = &pcb->mm_info->page_table->entries[page_num];
            entry->frame_number = ((uint8_t*)frame - phys_mem.data) / PAGE_SIZE;
            entry->present = 1;
            entry->read = 1;
            entry->write = 0;  // Kodea idatzi ezin da
            entry->execute = 1;
            entry->accessed = 0;
            entry->dirty = 0;
        }
        
        // Kodea idatzi memorian (force_write=1 kargatzeko)
        uint32_t paddr = translate_address_force(pcb->mm_info->page_table, vaddr, 1, 1);
        if (paddr == 0) {
            fprintf(stderr, "[LOADER] Errorea: ezin izan da kodea kargatu\n");
            destroy_page_table(pcb->mm_info->page_table);
            free(pcb->mm_info);
            free(pcb);
            return NULL;
        }
        uint32_t* mem_loc = (uint32_t*)(phys_mem.data + paddr);
        *mem_loc = prog->code[i];
    }
    
    // 3. Datu segmentua kargatu memorian
    pcb->mm_info->data_start = prog->data_start;
    for (uint32_t i = 0; i < prog->data_size; i++) {
        uint32_t vaddr = prog->data_start + (i * 4);
        uint32_t page_num = vaddr >> PAGE_SIZE_BITS;
        
        // Orri bat sortu behar bada
        if (!pcb->mm_info->page_table->entries[page_num].present) {
            void* frame = allocate_frame();
            if (!frame) {
                destroy_page_table(pcb->mm_info->page_table);
                free(pcb->mm_info);
                free(pcb);
                return NULL;
            }
            
            pte_t* entry = &pcb->mm_info->page_table->entries[page_num];
            entry->frame_number = ((uint8_t*)frame - phys_mem.data) / PAGE_SIZE;
            entry->present = 1;
            entry->read = 1;
            entry->write = 1;  // Datuak idatzi daitezke
            entry->execute = 0;
            entry->accessed = 0;
            entry->dirty = 0;
        }
        
        // Datuak idatzi memorian (force_write=1 kargatzeko)
        uint32_t paddr = translate_address_force(pcb->mm_info->page_table, vaddr, 1, 1);
        if (paddr == 0) {
            fprintf(stderr, "[LOADER] Errorea: ezin izan da datuak kargatu\n");
            destroy_page_table(pcb->mm_info->page_table);
            free(pcb->mm_info);
            free(pcb);
            return NULL;
        }
        uint32_t* mem_loc = (uint32_t*)(phys_mem.data + paddr);
        *mem_loc = prog->data[i];
    }
    
    // 4. PTBR eguneratu
    pcb->mm_info->ptbr = ((uint8_t*)pcb->mm_info->page_table - phys_mem.data);
    
    // Kontar zenbat frame erabilita dagoen
    int frames_used = 0;
    page_table_t* pt = pcb->mm_info->page_table;
    for (uint32_t i = 0; i < pt->num_entries; i++) {
        if (pt->entries[i].present) frames_used++;
    }
    
    printf("[LOADER] Prozesua sortu: PID=%d\n", pid);
    printf("  - Orri-taulak: %u sarrera\n", pcb->mm_info->page_table->num_entries);
    printf("  - Frame-ak erabilita: %d\n", frames_used);
    printf("  - PTBR: 0x%06X\n", pcb->mm_info->ptbr);
    
    return pcb;
}

/* Prozesaren memoria askatu (terminatzerakoan) */
void free_process_memory(pcb_t* proc) {
    if (!proc || !proc->mm_info) return;
    
    page_table_t* pt = proc->mm_info->page_table;
    if (!pt) return;
    
    // Orri-taulako sarrera bakoitza miatzea eta frame-ak askatu
    int frames_freed = 0;
    for (uint32_t i = 0; i < pt->num_entries; i++) {
        pte_t* entry = &pt->entries[i];
        if (entry->present) {
            // Frame helbidea lortu eta askatu
            uint32_t frame_addr = entry->frame_number * PAGE_SIZE;
            void* frame_ptr = phys_mem.data + frame_addr;
            free_frame(frame_ptr);
            frames_freed++;
            entry->present = 0;  // Markatu erabilezina
        }
    }
    
    // Orri-taula eta memoria informazioa askatu
    destroy_page_table(pt);
    free(proc->mm_info);
    proc->mm_info = NULL;
    
    printf("[LOADER] Prozesaren memoria askatu: PID=%d (%d frame-ak liberatu)\n", 
           proc->pid, frames_freed);
}