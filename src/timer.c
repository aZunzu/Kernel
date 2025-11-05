// timer.c - CORREGIDO según el esquema
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// ALDAGAI GLOBALAK
extern int exekutatzen;
extern pthread_mutex_t mutex;
extern pthread_cond_t cond;
extern pthread_cond_t cond2;
extern int done;
extern int tenp_kop;

void* timer_thread(void* arg) {
    int ticks_nahi = *(int*)arg;
    
    printf("⏰ TIMER abian: %d tick itxarongo\n", ticks_nahi);
    
    int tick_jaso = 0;
    
    // ✅ SEGÚN ESQUEMA: mutex_lock fuera del while
    pthread_mutex_lock(&mutex);
    
    while (exekutatzen && tick_jaso < ticks_nahi) {
        // ✅ SEGÚN ESQUEMA: done++ al principio
        done++;
        printf("[TIMER] done = %d, Tick %d/%d\n", done, tick_jaso + 1, ticks_nahi);
        
        // ✅ SEGÚN ESQUEMA: cond_signal(&cond) para despertar al clock
        pthread_cond_signal(&cond);
        
        // ✅ SEGÚN ESQUEMA: cond_wait(&cond2, &mutex) para esperar siguiente tick
        pthread_cond_wait(&cond2, &mutex);
        
        if (!exekutatzen) break;
        
        // Tick procesado
        tick_jaso++;
    }
    
    pthread_mutex_unlock(&mutex);
    
    if (tick_jaso >= ticks_nahi) {
        printf("⏰ TIMER: %d tick pasa dira!\n", ticks_nahi);
        usleep(500000);
        printf("⏰ TIMER: Lanak eginda!\n");
    }
    
    printf("⏰ TIMER gelditu da\n");
    return NULL;
}