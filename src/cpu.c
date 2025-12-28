#include <stdlib.h>
#include "cpu.h"
#include "config.h"

/* CPU, core eta hardware thread guztiak hasieratzen ditu */
void cpu_system_init(cpu_system_t* sys) {
    sys->cpu_kop = CPU_KOP;
    sys->core_kop = CORE_KOP;
    sys->hw_thread_kop = HW_THREAD_KOP;

    pthread_mutex_init(&sys->mutex, NULL);

    sys->cpus = malloc(sizeof(cpu_t) * sys->cpu_kop);

    for (int c = 0; c < sys->cpu_kop; c++) {
        sys->cpus[c].id = c;
        sys->cpus[c].cores = malloc(sizeof(core_t) * sys->core_kop);

        for (int i = 0; i < sys->core_kop; i++) {
            sys->cpus[c].cores[i].id = i;
            sys->cpus[c].cores[i].hw_threads =
                malloc(sizeof(hw_thread_t) * sys->hw_thread_kop);

            for (int h = 0; h < sys->hw_thread_kop; h++) {
                sys->cpus[c].cores[i].hw_threads[h].id = h;
                sys->cpus[c].cores[i].hw_threads[h].current_process = NULL;
            }
        }
    }
}
