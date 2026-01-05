#ifndef DEFINES_H
#define DEFINES_H

// Memoria birtualaren konfigurazioa 
#define VIRTUAL_BITS_DEFAULT 24        // 24 bit = 16 MB
#define PAGE_SIZE_BITS 12               // 2^12 = 4 KB
#define MAX_LINES_DEFAULT 20
#define PROG_NAME_DEFAULT "prog"
#define FIRST_NUMBER_DEFAULT 0
#define HOW_MANY_DEFAULT 60
#define VALUE 0x1000000                 // 2^24

// Instrukzio kodeak 
#define OPCODE_LD    0x0
#define OPCODE_ST    0x1
#define OPCODE_ADD   0x2
#define OPCODE_EXIT  0xF
//Hardware erregistro kopurua 
#define NUM_REGISTERS 16

// TLB tamaina 
#define TLB_ENTRIES 16

// Memoria fisikoaren tamaina 
#define PHYS_MEM_SIZE (1 << 24)        // 16 MB
#define PAGE_SIZE (1 << PAGE_SIZE_BITS) // 4 KB
#define NUM_FRAMES (PHYS_MEM_SIZE / PAGE_SIZE)

//Kernelak erreserbatutako frame kopurua 
#define KERNEL_FRAMES 1024              // 4 MB
#define USER_FRAME_START KERNEL_FRAMES

#endif