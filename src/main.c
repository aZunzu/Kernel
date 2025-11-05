#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"

void* clock_thread(void* arg);
void* timer_thread(void* arg);

int main() {
    printf("=== ESQUEMA EXACTO ===\n");
    printf("E: Clock, T: Timer\n");
    printf("Clock: %.1f Hz, Timer: %d tick\n\n", CLOCK_HZ, TIMER_TICKS);
    
    // ✅ Inicialización directa
    SharedData shared = {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .cond2 = PTHREAD_COND_INITIALIZER,
        .done = 0,
        .tenp_kop = TENP_KOP
    };
    
    ClockParams clock_params = {&shared, CLOCK_HZ};
    TimerParams timer_params = {&shared, TIMER_TICKS};
    
    pthread_t clock_tid, timer_tid;
    
    printf("Hariak sortzen...\n");
    
    // ✅ IMPORTANTE: Timer PRIMERO (necesita coger el mutex primero)
   pthread_create(&clock_tid, NULL, clock_thread, &clock_params);
   usleep(100000); // Asegurar que timer cogió el mutex
    pthread_create(&timer_tid, NULL, timer_thread, &timer_params);
    

    
    printf("Sistemak exekutatzen...\n");
    printf("Ctrl+\\ para gelditu\n\n");
    
    // ✅ Main infinito
    while (1) {
        sleep(1);
    }
    
    return 0;
}