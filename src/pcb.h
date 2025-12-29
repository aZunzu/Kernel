#ifndef PCB_H
#define PCB_H

/* Prozesuen egoerak */
typedef enum {
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} process_state_t;

/* Process Control Block */
typedef struct pcb {
    int pid;                // Prozesuaren ID-a
    int priority;           // 0 = normala, 1 = premiazkoa
    int exec_time;          // Exekuzio denbora totala
    int time_in_cpu;        // CPU-n egindako tick-ak
    int waiting_time;       // READY egoeran daraman denbora
    process_state_t state;  // Uneko egoera
    struct pcb* next;       // Ilara estekatzeko
} pcb_t;

/* PCB berri bat sortzen du */
pcb_t* pcb_create(int pid, int priority);

#endif
