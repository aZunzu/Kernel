#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"

void* clock_thread(void* arg) {
    ClockParams* params = (ClockParams*)arg;
    SharedData* shared = params->shared;
    double hz = params->hz;
    
    double periodoa = 1.0 / hz;
    printf("Erlojua abian: %.2f Hz\n", hz);

    int tick = 0;
    
    // ✅ INFINITO - sin exekutatzen
    while (1) {  
        pthread_mutex_lock(&shared->mutex);
        
        while(shared->done < shared->tenp_kop)
            pthread_cond_wait(&shared->cond, &shared->mutex);
        
        usleep(periodoa * 1000000);
        tick++;
        shared->sim_tick = tick;  // keep global tick in sync for display/logic
        shared->done = 0;
        
        if (tick % 5 == 0) {
            printf("[ERLOJUA] Tick %d\n", tick);
        }
        
        pthread_cond_broadcast(&shared->cond2);
        pthread_mutex_unlock(&shared->mutex);
    }

    // ❌ NUNCA LLEGA AQUÍ
    return NULL;
}