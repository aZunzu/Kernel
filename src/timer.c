#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"

void* timer_thread(void* arg) {
    TimerParams* params = (TimerParams*)arg;
    SharedData* shared = params->shared;
    int ticks_nahi = params->ticks_nahi;
    
    printf("⏰ TIMER abian: %d tick-rako konfiguratua\n", ticks_nahi);
    
    int tick_jaso = 0;
    int zikloak = 0;
    
    // ✅ ESQUEMA EXACTO
    pthread_mutex_lock(&shared->mutex);
    
    while (1) {
        shared->done++;
        tick_jaso++;
        zikloak++;
        
        printf("[TIMER] done=%d, Tick %d (meta:%d) [Ziklo:%d]\n", 
               shared->done, tick_jaso, ticks_nahi, zikloak);
        
        if (tick_jaso >= ticks_nahi) {
            printf("⏰ >>> LAN EGITEN %d. aldia <<<\n", zikloak/ticks_nahi);
            tick_jaso = 0;
        }
        
        pthread_cond_signal(&shared->cond);
        pthread_cond_wait(&shared->cond2, &shared->mutex);
    }
}