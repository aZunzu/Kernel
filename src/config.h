#ifndef CONFIG_H
#define CONFIG_H

#include <pthread.h>

// Estructura para datos compartidos
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_cond_t cond2;
    int done;
    int tenp_kop;
} SharedData;

// Estructura para parámetros del Clock
typedef struct {
    SharedData* shared;
    double hz;
} ClockParams;

// Estructura para parámetros del Timer  
typedef struct {
    SharedData* shared;
    int ticks_nahi;
} TimerParams;

// Configuraciones del sistema
#define CLOCK_HZ 1.0      // Frecuencia del clock
#define TIMER_TICKS 3     // Ticks para el timer
#define TENP_KOP 1        // Número de timers

#endif