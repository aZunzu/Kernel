// clock.c
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// Aldagai global sinplea
int exekutatzen = 1;
pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_cond_t cond2;
int done = 0;
int tenp_kop = 1;
// Erlojuaren hari nagusia - TICKAK BAKARRIK sortzen ditu
void* clock_thread(void* arg) {
    double hz = *(double*)arg;
    
    double periodoa = 1.0 / hz;
    printf("Erlojua abian: %.2f Hz\n", hz);

    int tick = 0;
    while (exekutatzen) {  
        pthread_mutex_lock(&mutex);
            while(done < tenp_kop)
                pthread_cond_wait(&cond, &mutex);
        usleep((useconds_t)(periodoa * 1000000));//zenbat segundo itxaron (*1000000 delako mikrosegundotan dagoelako)
        tick++;
        done = 0;
         // debug egiteko 10 tickero
        if (tick % 10 == 0) {
            printf("[ERLOJUA] Tick %d\n", tick);
        }
        pthread_cond_broadcast(&cond2);
        pthread_mutex_unlock(&mutex);

    }

    printf("Erlojua gelditu da. %d tick sortu dira\n", tick);
    return NULL;
    
}

