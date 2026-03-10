#include <stdio.h>
#include <stdalign.h>
#include <time.h>
#include "counter.h"

int main(int argc, char **argv) {
    /* Semilla para los números aleatorios */
    srand(time(NULL));

    /* Datos requeridos */
    double S[10];           // Array de resultados
    double *A;              // Puntero a los doubles a estudiar
    double ck[10];          // Número de ciclos contados
    double media = 0.0;     // Media de los experimentos
    double ckmedia = 0.0;   // Media de los ciclos medidos
    int R, D;               // Datos para estudiar la localidad [usamos R para referirnos tanto a L como a R]
    int i, j;               // Índices para los bucles

    /* Lectura de datos por terminal */
    D = atoi(argv[1]);
    R = atoi(argv[2]);  // Asumimos que se introducen valores correctos al emplear un script para ejecutar

    /* Calculamos R a partir de los valores de L y D leídos */
    // Se quiere acceder a L líneas diferentes y saltamos D posiciones entre datos, con lo que necesitamos acceder a R elementos de A[]
    if(D < 8){
        R = (8 * R) / D;    // Hacen falta 8/D saltos para cubrir una línea completa
    }
    // En el caso en el que D >= 8, con cada salto ya se accede a una línea caché diferente, con lo que R = L


    /* Inicializamos datos */
    A = (double*) aligned_alloc(64, R * D * sizeof(double));    // R*D doubles en rangos [1,2) o (-2,-1], alineando comienzo de A con el inicio de línea caché
    for(i = 0; i < R*D; i++){
        A[i] = (((double) rand()) / RAND_MAX) + 1;  // Valores aleatorios entre 0 y 1 trasladados a 1 y 2
        A[i] = A[i] * ((rand() % 2) * 2 - 1);       // Valores positivos (1*2-1) o negativos (0*2-1)
    }

    /* Código a medir */
    for(i = 0; i < 10; i++){        // 10 experimentos
        start_counter();            // Iniciamos contador

        S[i] = 0.0;
        for(j = 0; j <= R-1; j++){  // Recorremos R datos con salto D y los sumamos
            S[i]= S[i]+ A[j*D];     // Acceso directo a los datos
        }
        
        ck[i] = get_counter();      // Recibimos número de ciclos
    }
 
    /* Impresión de resultados */
    printf("\n######### D = %d\t\t R = %d #########\n", D, R);
    mhz(1,1);       // Frecuencia de reloj estimada (start_counter / get_counter)

    for(i = 0; i < 10; i++){    // Experimentos individuales con el número de ciclos correspondiente
        printf("\n Reducción[%d] = %1.10lf\n", i, S[i]);
        printf(" Ciclos[%d] = %1.10lf\n", i, ck[i]);

        media += S[i];
        ckmedia += ck[i];   // Contribución de los experimentos en cada iteración
    }

    printf("\n MEDIA EXPERIMENTOS = %1.10lf\n", media/10.0);
    printf(" CICLOS MEDIOS = %1.10lf\n", ckmedia/10.0);     // Impresión de medias pertinentes

    /* Liberar punteros */
    free(A);

    return 0;
}