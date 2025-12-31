#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"

void* timer_thread(void* arg) {
    TimerParams* params = (TimerParams*)arg;
    SharedData* shared = params->shared;
    int ticks_nahi = params->ticks_nahi;
    int id = params->id;
    char* izena = params->izena;
    
    double maiztasuna = (double)CLOCK_HZ / ticks_nahi;
    
    printf(" %s abian: %d tick (%.2f Hz)\n", izena, ticks_nahi, maiztasuna);
    
    int tick_jaso = 0;
    int zikloak = 0;
    
    pthread_mutex_lock(&shared->mutex);
    
    while (1) {
        // Clock-aren tick bat itxaron
        shared->done++;
        tick_jaso++;
        zikloak++;
        
        printf("[%s] done=%d, Tick %d/%d [Ziklo:%d]\n", 
               izena, shared->done, tick_jaso, ticks_nahi, zikloak);
        
        // Timer-ak bere periodoa betetzen duenean
        if (tick_jaso >= ticks_nahi) {
            printf(" %s >>> LAN EGITEN %d. aldia (%.2f Hz) <<<\n", 
                   izena, zikloak/ticks_nahi, maiztasuna);
            tick_jaso = 0;
            
            
            // Timer-ak scheduler-i jakinarazten dio bere lana egin dezan
            shared->scheduler_signal = 1;  // Seinalea piztu
            printf("   [%s] seinalea bidaltzen...\n", izena);
        }
        
        // Process Generator-ri jakinarazi (hau mantentzen da)
        pthread_cond_signal(&shared->cond);
        
        // Clock-aren hurrengo tick-a itxaron
        pthread_cond_wait(&shared->cond2, &shared->mutex);
    }
    
    return NULL;
}