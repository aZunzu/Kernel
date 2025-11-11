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
    int ticks_nahi;    // Cuántos ticks espera antes de "trabajar"
    int id;           // ID para identificar cada timer
    char* izena;      // ✅ Nombre del timer para debug
} TimerParams;

// Configuraciones del sistema
#define CLOCK_HZ 2.0      // Frecuencia del clock (más rápido)
#define TENP_KOP 3        // Número de timers

// ✅ FRECUENCIAS DIFERENTES para cada timer
#define TIMER1_TICKS 2    // Timer 1: 1 Hz (2 ticks / 2 Hz clock = 1 Hz)
#define TIMER2_TICKS 4    // Timer 2: 0.5 Hz (4 ticks / 2 Hz clock = 0.5 Hz)  
#define TIMER3_TICKS 6    // Timer 3: 0.33 Hz (6 ticks / 2 Hz clock = 0.33 Hz)

#endif