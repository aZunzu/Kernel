#include "hardware.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

/* MMU logging aukera (lehenetsita itzalita tick soilik moduan) */
int mmu_logs_enabled = 0;

/* MMU hasieratu */
void mmu_init(mmu_t* mmu) {
    // TLB hutsik
    for (int i = 0; i < TLB_ENTRIES; i++) {
        mmu->tlb[i].valid = 0;
    }
    mmu->tlb_index = 0;
    mmu->ptbr = 0;
    
    if (mmu_logs_enabled) {
        printf("[MMU] MMU hasieratuta hardware threadarentzat\n");
    }
}

/* Helbide birtuala itzuli helbide fisiko bihurtu */
uint32_t mmu_translate(mmu_t* mmu, uint32_t virtual_addr, int write, uint8_t* page_table) {
    // Orri zenbakia eta offset-a atera
    uint32_t page_num = virtual_addr >> PAGE_SIZE_BITS;
    uint32_t offset = virtual_addr & (PAGE_SIZE - 1);

    // 1. TLB bilaketa
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (mmu->tlb[i].valid && mmu->tlb[i].virtual_page == page_num) {
            // TLB hit: itzuli frame fisikoa
            uint32_t frame = mmu->tlb[i].physical_frame;
            if (write) mmu->tlb[i].dirty = 1;
            return (frame << PAGE_SIZE_BITS) | offset;
        }
    }

    // 2. TLB miss: orri-taula kontsultatu
    if (!page_table) {
        fprintf(stderr, "[MMU] Errorea: page_table NULL\n");
        return 0;
    }

    page_table_t* pt = (page_table_t*)page_table;
    if (page_num >= pt->num_entries) {
        fprintf(stderr, "[MMU] Errorea: orri zenbakia handiegia: %u\n", page_num);
        return 0;
    }

    pte_t* entry = &pt->entries[page_num];
    if (!entry->present) {
        fprintf(stderr, "[MMU] Errorea: orria ez dago memorian: %u\n", page_num);
        return 0;
    }
    if (write && !entry->write) {
        fprintf(stderr, "[MMU] Errorea: idazteko baimenik ez orrian: %u\n", page_num);
        return 0;
    }

    uint32_t frame = entry->frame_number;

    // 3. TLB eguneratu (FIFO)
    mmu->tlb[mmu->tlb_index].virtual_page = page_num;
    mmu->tlb[mmu->tlb_index].physical_frame = frame;
    mmu->tlb[mmu->tlb_index].valid = 1;
    mmu->tlb[mmu->tlb_index].dirty = write ? 1 : 0;

    mmu->tlb_index = (mmu->tlb_index + 1) % TLB_ENTRIES;

    // 4. Helbide fisikoa osatu
    return (frame << PAGE_SIZE_BITS) | offset;
}

/* TLB garbitu (prozesu aldaketa denean) */
void mmu_flush_tlb(mmu_t* mmu) {
    for (int i = 0; i < TLB_ENTRIES; i++) {
        mmu->tlb[i].valid = 0;
    }
    mmu->tlb_index = 0;
    
    if (mmu_logs_enabled) {
        printf("[MMU] TLB garbitu da\n");
    }
}

/* Hardware thread hasieratu */
void hw_thread_init(hw_thread_t* hw, int id) {
    hw->id = id;
    hw->pc = 0;
    hw->ir = 0;
    
    // Erregistroak zeroz hasieratu
    for (int i = 0; i < NUM_REGISTERS; i++) {
        hw->registers[i] = 0;
    }
    
    // MMU hasieratu
    mmu_init(&hw->mmu);
    
    printf("[HW_THREAD] Hardware thread %d hasieratuta\n", id);
}