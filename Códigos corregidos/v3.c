#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "counter.h"
#include <omp.h>

/* Definición de datos para el algoritmo */
#define TOL 1e-5
#define MAXITER 15000

// FORMATO DE EJECUCIÓN: ./v1 <tamaño vector> <número hilos> 
int main(int argc, char **argv){
	/* Establecer semilla */
	srand(1);	// la misma semilla para tener los mismos sistemas de ecuaciones

	/* Definición de datos necesarios */
	double **a;	    // matriz de coeficientes
	double *b;	    // vector de términos independientes
	double *x;	    // vector de soluciones
	double *x_new;	// vector de nueva solución
	
	// Datos que no necesitan reservas de memoria
	double ciclos;		// Número de ciclos medidos
	double norm2;		// Norma del vector al cuadrado
	double sigma;		// dato de apoyo para el algoritmo
    double resta;       // Variable auxiliar que permitirá calcular norm2
	int n = 0;		    // tamaño de los vectores
	int c = 1;		    // número de hilos
    int fin = 0;        // variable auxiliar para salir de los bucles en cada hilo
	
	/* Comprobar argumentos */
	if(argc < 2 || argc > 3){
		printf("Error. Número de argumentos incorrectos.\n");
		return 1;
	}
	
	// En caso de poner 2 o 3 argumentos, el primero será el n, y el segundo el c
	// Comprobamos que se hayan especificado tamaños positivos
	n = atoi(argv[1]);
	if(n <= 0) n = 1;
	if(argc == 3) c = atoi(argv[2]);
	if(c <= 0) c = 4;   // Por defecto, especificamos 4 hilos, que es valor con el que se tardan menos ciclos

	/* Reservas de memoria */
	// n columnas alineadas con el comienzo de línea de 64 bytes
	a = (double**) aligned_alloc(64, n * sizeof(double*));
	
	// Reservar memoria alineada para las filas de la matriz
	for(int i = 0; i < n; i++){
		a[i] = (double*) aligned_alloc(64, n * sizeof(double)); 
	}

	// Reservar memoria alineada para todos los demás vectores
	b = (double*) aligned_alloc(64, n * sizeof(double));
	x = (double*) aligned_alloc(64, n * sizeof(double));
	x_new = (double*) aligned_alloc(64, n * sizeof(double));

	/* Inicializar datos de los vectores y la matriz */
	for(int i = 0; i < n; i++){
		// Inicializamos elementos diagonal
		a[i][i] = (((double) rand()) / RAND_MAX) + 1;

		for(int j = 0; j < n; j++){
			// rand() produce números entre 0 y RAND_MAX (por eso dividimos)
			// Números aleatorios entre 0 y 1 trasladados a 1 y 2
			// Intentamos no sobreescribir el coeficiente diagonal
        	if(i != j) a[i][j] = (((double) rand()) / RAND_MAX) + 1;

			// Sumamos los elementos recién inicializados a la diagonal
			// Así, a será una matriz diagonal dominante
			if(i != j) a[i][i] += a[i][j];
		}

		// Inicializamos los elementos del vector b (también entre 1 y 2)
		b[i] = (((double) rand()) / RAND_MAX) + 1; 
		
		// Estimación de la solución (todo a cero)
		x[i] = 0.0;
    }

    /*
    * ==============================================================================
    * SECCIÓN DEL CÓDIGO DESTINADA A LOS CÁLCULOS
    * ==============================================================================
    */
	/* Iniciar contador de ciclos para el algoritmo */
	start_counter();

    /*
    * sigma y resta serán las únicas variables privadas porque son datos auxiliares,
    * pero son necesarios para calcular otros valores, por lo que necesitamos evitar carreras críticas con estos
    */
    // No necesitamos indicar a ni b en shared o private debido a que no se modificarán, y lo mismo para n
    #pragma omp parallel num_threads(c) private(sigma, resta)
    {
        /* Bucle para el método de Jacobi */
        for(int iter = 0; iter < MAXITER; iter++){
            // Reiniciamos valor de la norma
            // Solo se hace desde uno de los hilos, de ahí el uso de single
            #pragma omp single
            norm2 = 0.0;
            ///////// Barrera implícita que impedirá la continuación de la ejecución de los hilos hasta que 1 de ellos ejecute el bloque single

            /*
            * Si queremos conocer el número de procesadores empleados:
            * printf("%d\n", omp_get_num_procs());
            * No lo haremos porque ya especificamos el num al ejecutar en el CESGA
            */

            /* Bucle de cálculos */
            /* 
            * Preferimos static sobre dynamic o guided porque siempre se harán las mismas operaciones, que tardarán aproximadamente lo mismo
            * Es decir, no es necesario añadir sobrecargas por asignaciones dinámicas
            */
            /*
            * Reduction mejor que critical porque así no detenemos la ejecución de los hilos para hacer un simple cálculo
            * Debemos indicar qué operación se hará con la variable local especificada (en este caso sumar todos los norm)
            */
            // Dividimos las iteraciones entre los c hilos
            // Cada iteración escribe en una posición distinta de x_new, lo que evita condiciones de carrera
            #pragma omp for schedule(static) reduction(+:norm2)
            // Bucle para la siguiente iteración de la solución
            for(int i = 0; i < n; i++){
                // Reiniciamos valor de sigma
                sigma = 0.0;

                // Subbucle para incrementar sigma
                // Cada hilo posee su propia copia de sigma, con lo que se evitan condiciones de carrera
                for(int j = 0; j < n; j++){
                    if(i != j) sigma += a[i][j] * x[j];
                }

                // Calculamos el nuevo elemento de x
                x_new[i] = (b[i] - sigma) / a[i][i];

                // Cada norm2 local se acumula para ser combinado al final mediante la reducción
                // Comprobamos la diferencia al cuadrado entre soluciones
                resta = x_new[i] - x[i];
                norm2 += resta * resta;
            }   // Fin región paralelizada
            ///////// Barrera implícita que sincronizará los hilos después de que todos ejecuten el bucle previo

            /* Bucle de actualización de x */
            // En este caso es preferible usar static sobre dynamic o guided debido a que solo tenemos una operación que siempre tardará lo mismo
            // No es conveniente añadir una sobrecarga ocasionada por asignaciones de trabajo a hilos de forma variable
            // Cada hilo modifica un x distinto, con lo que no se producen sobreescrituras
            #pragma omp for schedule(static)
            // Actualizamos el vector x
            for(int k = 0; k < n; k++){
                x[k] = x_new[k];
            }   // Fin región paralelizada
            ///////// Barrera implícita que sincronizará los hilos después de que todos ejecuten el bucle previo

            // Comprobamos que se ha alcanzado la tolerancia
            // sqrtl para la mayor precisión posible
            // No es posible usar break en este caso, puesto que solo se saldría de un hilo, y el resto seguirían ejecutándose, cuando ya se tiene la solución
            // Para hacer que todos los hilos salgan del bucle, empleamos una variable auxiliar
            #pragma omp single
            if(sqrtl(norm2) < TOL) fin = 1;
            ///////// Barrera implícita que sincronizará los hilos tras actualizar fin (o no actualizarlo pero terminar de ejecutar el bloque single)

            // Salimos de los bucles en caso de alcanzar la tolerancia
            // Gracias al single que pone fin a 1 (y a la barrera implícita pertinente), todos los hilos tendrán la variable actualizada
            if(fin) break;
        }
    }   // Fin de la región total paralelizada

	/* Medir número de ciclos al terminar */
	ciclos = get_counter(); 
    /*
    * ==============================================================================
    */

	/* Impresión de resultados */
	// Tamaño de los vectores
	// Norma al cuadrado con 15 decimales
	// Número de ciclos medidos
	// Número de hilos
	printf("\nv3 \t %d \t %.15f \t %lf \t %d \n", n, norm2, ciclos, c);
	
	/* Liberar variables */
	for(int i = 0; i < n; i++) free(a[i]);
	free(a);
	free(b);
	free(x);
	free(x_new);

	return 0;
}

// COMPILACIÓN: gcc -o v3 v3.c -lm -fopenmp
// Es importante destacar que especificar un mayor número de hilos no resulta en una mayor velocidad (el tiempo óptimo se logra con aproximadamente 4 hilos)
    // Esto se debe, probablemente, a que con muchos hilos existen demasiados datos en caché y se producen muchos fallos de acceso