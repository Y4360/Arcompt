#ifndef COUNTER_H
#define COUNTER_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


// void start_counter();
// double get_counter();
// double mhz(int verbose, int sleeptime);


/* Inicializar contador de ciclos */

static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;

/* *hi y *lo establecidos al mayor y menor orden de bits del contador de ciclos
Se requiere código assembly para usar la instrucción rdtsc */
static inline void access_counter(unsigned *hi, unsigned *lo) {
    asm("rdtsc; movl %%edx,%0; movl %%eax,%1" /* Leer contador de ciclos */
            : "=r" (*hi), "=r" (*lo) /* y mover resultados a */
            : /* Ninguna entrada */ /* las dos salidas */
            : "%edx", "%eax");
}
/* La opción inline sustituye el código de la función por su contenido al compilar,
es decir, al escribir access_counter(...) en un programa, al compilar se pondrá el asm(...) */

/* asm permite usar lenguaje ensamblador. En este caso, lo que se hace es emplear rdtsc, 
que devuelve el número de ciclos de reloj desde el último reinicio del procesador en un valor de 64 bits,
y guarda los valores mínimos y máximos en *lo y *hi respectivamente */

/* Guardar / grabar el valor actual del contador de ciclos */
static inline void start_counter() {
    access_counter(&cyc_hi, &cyc_lo);
}

/* Devolver el número de ciclos desde la última llamada a start_counter */
static inline double get_counter() {
    unsigned ncyc_hi, ncyc_lo;
    unsigned hi, lo, borrow;
    double result;

    /* Obtener el contador de ciclos */
    access_counter(&ncyc_hi, &ncyc_lo);

    /* Restar en doble precisión (los ciclos actuales menos los del último acceso a start_counter) */
    lo = ncyc_lo - cyc_lo;
    borrow = lo > ncyc_lo;  // Si la diferencia de LSB es mayor que el último valor medido, nos llevamos 1
    hi = ncyc_hi - cyc_hi - borrow; // Restamos los MSB, teniendo en cuenta el valor que arrastramos de lo
    result = (double) hi * (1 << 30) * 4 + lo;  // Desplazamos 32 bits a la izquierda hi y lo juntamos con lo
    if (result < 0) {
        fprintf(stderr, "Error: counter returns neg value: %.0f\n", result);
    }
    return result;
}

/* Devolver los MHz del procesador, mediante los números de ciclos
entre dos momentos temporales (distanciados en sleeptime segundos) */
static inline double mhz(int verbose, int sleeptime) {
    double rate;

    start_counter();
    sleep(sleeptime);
    rate = get_counter() / (1e6 * sleeptime);   // Obtenemos la diferencia de ciclos y hacemos media con el tiempo de espera
    if (verbose) {
        printf("\n Processor clock rate = %.1f MHz\n", rate);
    }
    return rate;
}

#endif //COUNTER_H