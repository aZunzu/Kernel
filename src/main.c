#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// Aldagai globalak (clock.c eta timer.c erabiliko dituzte)
extern int exekutatzen;
extern pthread_mutex_t mutex;
extern pthread_cond_t cond;
extern pthread_cond_t cond2;
extern int done;
extern int tenp_kop;
// Funtzioak deklaratu
void* clock_thread(void* arg);
void* timer_thread(void* arg);

int main() {
    printf("=== SINKRONIZAZIOA IKUSTEN ===\n");
    
    // Mutex eta condition variables hasieratu
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_cond_init(&cond2, NULL);
    
    // Konfigurazioa
    double clock_hz = 1.0;  // 1 Hz (segundo 1)
    int timer_ticks = 3;    // 3 tick itxarongo ditu
    
    // Hariak sortu
    pthread_t clock_tid, timer_tid;
    
    printf("CLOCK eta TIMER sortzen...\n");
    pthread_create(&clock_tid, NULL, clock_thread, &clock_hz);
    pthread_create(&timer_tid, NULL, timer_thread, &timer_ticks);
    
    printf("MAIN-ak itxaroten timer-ak amaitu arte...\n");
    pthread_join(timer_tid, NULL);
    
    printf("TIMER amaitu da. CLOCK gelditzen...\n");
    exekutatzen = 0;
    pthread_join(clock_tid, NULL);
    
    printf("✅ Sinkronizazioa eginda!\n");
    return 0;
}