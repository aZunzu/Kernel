#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// ALDAGAI GLOBALAK (clock.c-ren berberak)
extern int exekutatzen;
extern pthread_mutex_t mutex;
extern pthread_cond_t cond;
extern pthread_cond_t cond2;
extern int done;
extern int tenp_kop;

// Timer-aren hari nagusia
void* timer_thread(void* arg) {
    int ticks_nahi = *(int*)arg;  // Zenbat tick itxarongo dituen
    
    printf("⏰ TIMER abian: %d tick itxarongo\n", ticks_nahi);
    
    int tick_jaso = 0;
    
    pthread_mutex_lock(&mutex);
    
    while (exekutatzen && tick_jaso < ticks_nahi) {
        // ITXARON CLOCK-aren TICK BATERA
        pthread_cond_wait(&cond, &mutex);
        
        if (!exekutatzen) break;
        
        // TICK bat jaso du
        tick_jaso++;
        printf("[TIMER] Tick %d/%d jaso\n", tick_jaso, ticks_nahi);
        
        // CLOCK-RI jakinarazi timer gehigarri bat dagoela
        done++;
        pthread_cond_signal(&cond2);
        
        // ITXARON HURRENGO TICK-ERA
        pthread_cond_wait(&cond2, &mutex);
    }
    
    pthread_mutex_unlock(&mutex);
    
    if (tick_jaso >= ticks_nahi) {
        printf("⏰ TIMER: %d tick pasa dira! Mikrosegundo batzuk itxarongo...\n", ticks_nahi);
        usleep(500000); // 0.5 segundo itxaron (adibidez)
        printf("⏰ TIMER: Lanak eginda!\n");
    }
    
    printf("⏰ TIMER gelditu da\n");
    return NULL;
}