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
	double **a;	// matriz de coeficientes
	double *b;	// vector de términos independientes
	double *x;	// vector de soluciones
	double *x_new;	// vector de nueva solución
	
	// Datos que no necesitan reservas de memoria
	double ciclos;		// Número de ciclos medidos
	double norm2;		// Norma del vector al cuadrado
	double sigma;		// dato de apoyo para el algoritmo
	int n = 0;		// tamaño de los vectores
	int c = 1;		// número de hilos
	int i=0, k=0, j=0;	// índices para los bucles
	
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
	if(c <= 0) c = 1;

	/* Reservas de memoria */
	// Matriz n * n alineada con el comienzo de línea de 64 bytes
	a = (double**) aligned_alloc(64, n * n * sizeof(double));
	
	// Reservar memoria alineada para las filas de la matriz
	for(i = 0; i < n; i++){
		a[i] = (double*) aligned_alloc(64, n * sizeof(double)); 
	}

	// Reservar memoria alineada para todos los demás vectores
	b = (double*) aligned_alloc(64, n * sizeof(double));
	x = (double*) aligned_alloc(64, n * sizeof(double));
	x_new = (double*) aligned_alloc(64, n * sizeof(double));

	/* Inicializar datos de los vectores y la matriz */
	for(i = 0; i < n; i++){
		// Inicializamos elementos diagonal
		a[i][i] = (((double) rand()) / RAND_MAX) + 1;

		for(j = 0; j < n; j++){
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

	/* Iniciar contador de ciclos para el algoritmo */
	start_counter();

	/* Bucle para el método de Jacobi */
	for(int iter = 0; iter < MAXITER; iter++){
		// Reiniciamos valor de la norma
		norm2 = 0.0;

		#pragma omp parallel private(sigma,i,j) shared(a,x,b,x_new,n) num_threads(c)
		{
			// Si queremos conocer el número de procesadores empleados:
			// printf("%d\n", omp_get_num_procs());
			// No lo haremos porque ya especificamos el num al ejecutar en el CESGA
	
			// Dividimos las iteraciones entre los c hilos
			// Cada iteración del bucle se corresponde con el cálculo de un x, por lo que no habrá carreras críticas
			#pragma omp for schedule(dynamic,c)
				// Bucle para la siguiente iteración de la solución
				for(i = 0; i < n; i++){
					// Reiniciamos valor de sigma
					sigma = 0.0;

					// Subbucle para incrementar sigma
					// Sigma es privado, con lo que no se producirán carreras críticas
					// Forman parte de una sola línea, con lo que ya no hay riesgo de escrituras
					for(j = 0; j < n; j++){
						if(i != j) sigma += a[i][j] * x[j];
					}

					// Calculamos el nuevo elemento de x
					x_new[i] = (b[i] - sigma) / a[i][i];
		
					// Copias locales de norm2 se combinan al final en la variable global
					#pragma omp reduction
						// Comprobamos la diferencia entre soluciones
						// Elevamos a 2.0000 para hacer los cálculos en doble precisión
						norm2 += pow((x_new[i] - x[i]), 2.0000);
				}
		
		}	// Fin de ejecución paralela


		#pragma omp parallel private(k) shared(x,x_new,n) num_threads(c)
		{
			#pragma omp for schedule(dynamic,c)
				// Actualizamos el vector x
				for(k = 0; k < n; k++) x[k] = x_new[k];
		}

		// Comprobamos que se ha alcanzado la tolerancia (salimos si es así)
		// sqrtl para la mayor precisión posible
		if(sqrtl(norm2) < TOL) break;
	}

	/* Medir número de ciclos al terminar */
	ciclos = get_counter(); 

	/* Impresión de resultados */
	printf("\n%.15f\n", norm2);	// Impresión de la norma al cuadrado (15 decimales)
	// printf("\nSolución calculada:\n");
	// for(int k = 0; k < n; k++) printf("x[%d] = %lf\n", k, x[k]);
	
	printf("\n%lf\n", ciclos);	// Imprimir ciclos medidos

	/* Liberar variables */
	for(i = 0; i < n; i++) free(a[i]);
	free(a);
	free(b);
	free(x);
	free(x_new);

	return 0;
}

// COMPILACIÓN: gcc -O0 v3.c -lm -fopenmp

// Para percibir el paralelismo se necesita elegir un tamaño de vectores (n) lo suficientemente grande (eg, n >= 1000)
