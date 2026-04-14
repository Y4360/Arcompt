#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "counter.h"

/* Definición de datos para el algoritmo */
#define TOL 1e-5
#define MAXITER 15000

int main(int argc, char **argv){
	/* Establecer semilla */
	srand(1);   // la misma semilla para tener los mismos sistemas de ecuaciones

	/* Definición de datos necesarios */
	double **a;	    // matriz de coeficientes
	double *b;	    // vector de términos independientes
	double *x;	    // vector de soluciones
	double *x_new;	// vector de nueva solución
	
	// Datos que no necesitan reservas de memoria
	double ciclos;	// Número de ciclos medidos
	double norm2;	// Norma del vector al cuadrado
	double sigma;	// dato de apoyo para el algoritmo
    double resta;   // Variable auxiliar que permitirá calcular norm2
	int n = 0;	    // tamaño de los vectores
	
	/* Comprobación de argumentos */
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
		// Inicializamos elementos diagonales
		a[i][i] = (((double) rand()) / RAND_MAX) + 1;

		for(int j = 0; j < n; j++){
			// rand() produce números entre 0 y RAND_MAX (por eso dividimos)
			// Generamos números aleatorios entre 0 y 1 y los trasladamos a 1 y 2
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

	/* Bucle para el método de Jacobi */
	for(int iter = 0; iter < MAXITER; iter++){
		// Reiniciamos valor de la norma
		norm2 = 0.0;

		// Bucle para la siguiente iteración de la solución
		for(int i = 0; i < n; i++){
			// Reiniciamos valor de sigma
			sigma = 0.0;

			// Subbucle para incrementar sigma
			for(int j = 0; j < n; j++){
                // Ignoramos el elemento diagonal
				if(i!=j) sigma += a[i][j] * x[j];
			}

			// Calculamos el nuevo elemento de x
            // Como a es diagonal dominante con valores positivos, tenemos garantizado que a[i][i] es distinto de 0
			x_new[i] = (b[i] - sigma) / a[i][i];
			
			// Comprobamos la diferencia al cuadrado entre soluciones
            resta = x_new[i] - x[i];
			norm2 += resta * resta;
		}
		
		// Actualizamos el vector x
		for(int k = 0; k < n; k++) x[k] = x_new[k];

		// Comprobamos que se ha alcanzado la tolerancia (salimos si es así)
		// sqrtl para la mayor precisión posible
		if(sqrtl(norm2) < TOL) break;
	}

	/* Medir número de ciclos al terminar */
	ciclos = get_counter(); 
    /*
    * ==============================================================================
    */

	/* Impresión de resultados */
	// Tamaño de los vectores
	// Norma al cuadrado con 15 decimales
	// Número de ciclos medidos
	// Número de hilos (en este programa solo 1)
	printf("\nv1 \t %d \t %.15f \t %lf \t 1 \n", n, norm2, ciclos);
	
	/* Liberar variables */
	for(int i = 0; i < n; i++) free(a[i]);
	free(a);
	free(b);
	free(x);
	free(x_new);

	return 0;
}

// COMPILACIÓN: gcc -o v1 v1.c -lm
// FORMATO DE EJECUCIÓN: ./v1 <tamaño vector> <número hilos> 