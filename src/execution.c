#include "execution.h"
#include "memory.h"
#include <stdio.h>

// Hitza irakurri memoria fisikotik 
static uint32_t read_word(uint32_t physical_addr) {
    if (physical_addr >= PHYS_MEM_SIZE) {
        fprintf(stderr, "[EXECUTION] Errorea: helbide fisikoa handiegia: 0x%06X\n", physical_addr);
        return 0;
    }
    
    uint32_t* addr = (uint32_t*)(phys_mem.data + physical_addr);
    return *addr;
}

// Hitza idatzi memoria fisikoan 
static void write_word(uint32_t physical_addr, uint32_t value) {
    if (physical_addr >= PHYS_MEM_SIZE) {
        fprintf(stderr, "[EXECUTION] Errorea: helbide fisikoa handiegia: 0x%06X\n", physical_addr);
        return;
    }
    
    uint32_t* addr = (uint32_t*)(phys_mem.data + physical_addr);
    *addr = value;
}

//Exekuzio pauso bat (instrukzio bat) 
int execute_step(hw_thread_t* hw, pcb_t* process) {
    // Instrukzioa kargatu PC helbidetik
    uint32_t vaddr = process->pc;
    uint32_t paddr = mmu_translate(&hw->mmu, vaddr, 0, 
                                   (uint8_t*)process->mm_info->page_table);
    
    if (paddr == 0) {
        fprintf(stderr, "[EXECUTION] Errorea: ezin izan da PC helbidea itzuli: 0x%06X\n", vaddr);
        return -1;
    }
    
    hw->ir = read_word(paddr);
    hw->pc = vaddr + 4;
    process->pc += 4;
    
    // Instrukzioa dekodetu
    uint8_t opcode = (hw->ir >> 28) & 0xF;
    uint8_t reg1 = (hw->ir >> 24) & 0xF;
    uint8_t reg2 = (hw->ir >> 20) & 0xF;
    uint8_t reg3 = (hw->ir >> 16) & 0xF;
    uint32_t address = hw->ir & 0xFFFFFF;
    
    // Instrukzioa exekutatu
    switch (opcode) {
        case OPCODE_LD: {  // ld R, [addr]
            uint32_t data_vaddr = address;
            uint32_t data_paddr = mmu_translate(&hw->mmu, data_vaddr, 0,
                                               (uint8_t*)process->mm_info->page_table);
            
            if (data_paddr == 0) {
                fprintf(stderr, "[EXECUTION] Errorea: ezin izan da data helbidea itzuli: 0x%06X\n", data_vaddr);
                return -1;
            }
            
            uint32_t data = read_word(data_paddr);
            hw->registers[reg1] = data;
            
            printf("[EXECUTION] PID %d: ld R%d, [0x%06X] -> R%d = 0x%08X\n",
                   process->pid, reg1, data_vaddr, reg1, data);
            break;
        }
        
        case OPCODE_ST: {  // st R, [addr]
            uint32_t data_vaddr = address;
            uint32_t data_paddr = mmu_translate(&hw->mmu, data_vaddr, 1,
                                               (uint8_t*)process->mm_info->page_table);
            
            if (data_paddr == 0) {
                fprintf(stderr, "[EXECUTION] Errorea: ezin izan da data helbidea itzuli: 0x%06X\n", data_vaddr);
                return -1;
            }
            
            write_word(data_paddr, hw->registers[reg1]);
            
            printf("[EXECUTION] PID %d: st R%d, [0x%06X] -> [0x%06X] = 0x%08X\n",
                   process->pid, reg1, data_vaddr, data_vaddr, hw->registers[reg1]);
            break;
        }
        
        case OPCODE_ADD: {  // add Rd, Rs1, Rs2
            hw->registers[reg1] = hw->registers[reg2] + hw->registers[reg3];
            
            printf("[EXECUTION] PID %d: add R%d, R%d, R%d -> R%d = 0x%08X\n",
                   process->pid, reg1, reg2, reg3, reg1, hw->registers[reg1]);
            break;
        }
        
        case OPCODE_EXIT: {  // exit
            printf("[EXECUTION] PID %d: exit agindua exekutatu da\n", process->pid);
            process->exit_code = 0;
            return 0;  // Prozesua amaitu da
        }
        
        default:
            fprintf(stderr, "[EXECUTION] Errorea: opcode ezezaguna: 0x%X\n", opcode);
            return -1;
    }
    
    return 1;  // Instrukzioa ondo exekutatu da
}

// Erregistroak erakutsi 
void print_registers(hw_thread_t* hw) {
    printf("=== ERREGISTROAK (HW Thread %d) ===\n", hw->id);
    for (int i = 0; i < NUM_REGISTERS; i += 4) {
        printf("R%02d: 0x%08X  R%02d: 0x%08X  R%02d: 0x%08X  R%02d: 0x%08X\n",
               i, hw->registers[i],
               i+1, hw->registers[i+1],
               i+2, hw->registers[i+2],
               i+3, hw->registers[i+3]);
    }
    printf("PC: 0x%06X  IR: 0x%08X\n", hw->pc, hw->ir);
}

// Memoria tarte bat erakutsi 
void print_memory_range(uint32_t start, uint32_t end) {
    printf("=== MEMORIA (0x%06X - 0x%06X) ===\n", start, end);
    
    for (uint32_t addr = start; addr <= end && addr < PHYS_MEM_SIZE; addr += 16) {
        printf("0x%06X: ", addr);
        for (int i = 0; i < 16 && (addr + i) < PHYS_MEM_SIZE; i++) {
            printf("%02X ", phys_mem.data[addr + i]);
        }
        printf(" | ");
        for (int i = 0; i < 16 && (addr + i) < PHYS_MEM_SIZE; i++) {
            unsigned char c = phys_mem.data[addr + i];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    }
}