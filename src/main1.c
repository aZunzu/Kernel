#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include "config.h"

void* clock_thread(void* arg);
void* timer_thread(void* arg);

int main() {
    printf("=== TIMERRAK MAIZTASUN DESBERDINEKIN ===\n");
    printf("Clock: %.1f Hz, %d timer\n\n", CLOCK_HZ, TENP_KOP);
    
    SharedData shared = {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .cond2 = PTHREAD_COND_INITIALIZER,
        .done = 0,
        .tenp_kop = TENP_KOP
    };
    
    ClockParams clock_params = {&shared, CLOCK_HZ};
    
    pthread_t clock_tid;
    pthread_t timer_tid[TENP_KOP];
    TimerParams timer_params[TENP_KOP];
    
    printf("Hariak sortzen...\n");
    
    // Clock primero
    pthread_create(&clock_tid, NULL, clock_thread, &clock_params);
    usleep(100000);
    
    // ✅ Timer 1: Rápido (1 Hz)
    timer_params[0].shared = &shared;
    timer_params[0].ticks_nahi = TIMER1_TICKS;
    timer_params[0].id = 1;
    timer_params[0].izena = "TIMER RAPIDOA";
    
    // ✅ Timer 2: Medio (0.5 Hz)  
    timer_params[1].shared = &shared;
    timer_params[1].ticks_nahi = TIMER2_TICKS;
    timer_params[1].id = 2;
    timer_params[1].izena = "TIMER ERDIA";
    
    // ✅ Timer 3: Lento (0.33 Hz)
    timer_params[2].shared = &shared;
    timer_params[2].ticks_nahi = TIMER3_TICKS;
    timer_params[2].id = 3;
    timer_params[2].izena = "TIMER MANTSOA";
    
    // Crear todos los timers
    for (int i = 0; i < TENP_KOP; i++) {
        pthread_create(&timer_tid[i], NULL, timer_thread, &timer_params[i]);
        usleep(50000);
    }
    
    printf("\n⏰ Timer motak:\n");
    printf("- %s: %d tick (%.2f Hz)\n", timer_params[0].izena, TIMER1_TICKS, (double)CLOCK_HZ/TIMER1_TICKS);
    printf("- %s: %d tick (%.2f Hz)\n", timer_params[1].izena, TIMER2_TICKS, (double)CLOCK_HZ/TIMER2_TICKS);
    printf("- %s: %d tick (%.2f Hz)\n", timer_params[2].izena, TIMER3_TICKS, (double)CLOCK_HZ/TIMER3_TICKS);
    printf("\nSistemak exekutatzen...\n");
    printf("Ctrl+\\ para gelditu\n\n");
    
    while (1) {
        sleep(1);
    }
    
    return 0;
}