#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "counter.h"

/* Definición de datos para el algoritmo */
#define TOL 1e-5
#define MAXITER 15000
#define BLOQUE 4	// Así, tendremos 4 doubles de a[][] y 4 de x[] ocupando los 64 bytes de la caché
			// Con un valor superior a 4 se producen segmentation faults

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
	double ciclos;	// Número de ciclos medidos
	double norm2;	// Norma del vector al cuadrado
	double sigma;	// dato de apoyo para el algoritmo
	int n = 0;	// tamaño de los vectores
	
	/* Comprobar argumentos */
	if(argc < 2 || argc > 3){
		printf("Error. Número de argumentos incorrectos.\n");
		return 1;
	}
	
	// En caso de poner 2 o 3 argumentos, el primero será el n
	// El otro argumento, ie, el número de hilos, se omitirá en esta versión
	// Comprobamos que se haya especificado un tamaño positivo
	n = atoi(argv[1]);
	if(n <= 0) n = 1;

	/* Reservas de memoria */
	// Matriz n * n alineada con el comienzo de línea de 64 bytes
	a = (double**) aligned_alloc(64, n * n * sizeof(double));
	
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

	/* Iniciar contador de ciclos para el algoritmo */
	start_counter();

	/* Bucle para el método de Jacobi */
	for(int iter = 0; iter < MAXITER; iter++){
		// Reiniciamos valor de la norma
		norm2 = 0.0;

		// Bucle para la siguiente iteración de la solución
		for(int i = 0; i < n; i+=BLOQUE){
			for(int ii = i; ii < i + BLOQUE; ii++){
				// Reiniciamos valor de sigma
				sigma = 0.0;

				// Subbucle para incrementar sigma
				for(int j = 0; j < n; j+=BLOQUE){
					for(int jj = j; jj < j + BLOQUE; jj++){
						if(ii != jj) sigma += a[ii][jj] * x[jj];
					}
				}

				// Calculamos el nuevo elemento de x
				x_new[ii] = (b[ii] - sigma) / a[ii][ii];
			
				// Comprobamos la diferencia entre soluciones
				// Elevamos a 2.0000 para hacer los cálculos en doble precisión
				norm2 += pow((x_new[ii] - x[ii]), 2.0000);
			}
		}
		
		// Actualizamos el vector x
		for(int k = 0; k < n; k+=BLOQUE){
			for(int kk = k; kk < k + BLOQUE; kk++){
				x[kk] = x_new[kk];
			}
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
	for(int i = 0; i < n; i++) free(a[i]);
	free(a);
	free(b);
	free(x);
	free(x_new);

	return 0;
}

// COMPILACIÓN: gcc -O0 v1.c -lm
