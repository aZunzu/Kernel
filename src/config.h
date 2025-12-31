#ifndef CONFIG_H
#define CONFIG_H

#include <pthread.h>

/* Partekatutako datuen egitura */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;     // Clock -> Timer sinkronizaziorako
    pthread_cond_t cond2;    // Scheduler-erako (orain Timer-ek erabiltzen dute)
    int done;
    int tenp_kop;
    int sim_running;
    int sim_tick;
    int scheduler_signal;    //  Timer-ak Scheduler-i jakinarazteko
} SharedData;

/* Clock-aren parametroak */
typedef struct {
    SharedData* shared;
    double hz;
} ClockParams;

/* Timer-aren parametroak */
typedef struct {
    SharedData* shared;
    int ticks_nahi;
    int id;
    char* izena;
} TimerParams;

/* Clock eta Timer hari funtzioak */
void* clock_thread(void* arg);
void* timer_thread(void* arg);

/* Sistemaren konfigurazioa */
#define CLOCK_HZ 2.0
#define TENP_KOP 3

#define TIMER1_TICKS 2
#define TIMER2_TICKS 4
#define TIMER3_TICKS 6

/* CPU arkitekturaren konfigurazioa */
#define CPU_KOP 1
#define CORE_KOP 2
#define HW_THREAD_KOP 2

#endif