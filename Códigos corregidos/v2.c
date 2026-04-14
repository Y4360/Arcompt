#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "counter.h"

/* Definición de datos para el algoritmo */
#define TOL 1e-5
#define MAXITER 15000

int main(int argc, char **argv){
	/* Establecer semilla */
	srand(1);	// la misma semilla para tener los mismos sistemas de ecuaciones

	/* Definición de datos necesarios */
	double **a;	    // matriz de coeficientes
	double *b;	    // vector de términos independientes
	double *x;	    // vector de soluciones
	double *x_new;	// vector de nueva solución
	
	// Datos que no necesitan reservas de memoria
	double ciclos;	// Número de ciclos medidos
	double norm2;	// Norma del vector al cuadrado
    double resta;   // Variable auxiliar que permitirá calcular norm2
	int n = 0;	    // tamaño de los vectores

	// datos de apoyo para el algoritmo
    double sigma0, sigma1, sigma2, sigma3;
	
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

    /* Cálculo de los índices que facilitarán la división de lazos */
    double total_mult8;
    // Primero, recorremos un múltiplo de 8 elementos de los vectores (para 2 bucles con saltos de 4 en 4)
    // Posteriormente, los últimos 1 a 7 datos se recorren aparte
    total_mult8 = n - (n%8);

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

		// Bucle 1 para la siguiente iteración de la solución
		// total_mult8 ya ha sido definido para ser múltiplo de 8, con lo que no nos saliremos de los arrays
		for(int i = 0; i < total_mult8/2; i+=4){
			// Reiniciamos valores de sigma
			sigma0 = 0.0;
			sigma1 = 0.0;
			sigma2 = 0.0;
			sigma3 = 0.0;

			// Subbucle para incrementar sigma (teniendo en cuenta los límites)
			for(int j = 0; j < total_mult8; j+=4){
				// Desenrrollamos el lazo
				sigma0 += a[i][j] * x[j];
				sigma0 += a[i][j+1] * x[j+1];
				sigma0 += a[i][j+2] * x[j+2];
				sigma0 += a[i][j+3] * x[j+3];

				sigma1 += a[i+1][j] * x[j];
				sigma1 += a[i+1][j+1] * x[j+1];
				sigma1 += a[i+1][j+2] * x[j+2];
				sigma1 += a[i+1][j+3] * x[j+3];
				
				sigma2 += a[i+2][j] * x[j];
				sigma2 += a[i+2][j+1] * x[j+1];
				sigma2 += a[i+2][j+2] * x[j+2];
				sigma2 += a[i+2][j+3] * x[j+3];

				sigma3 += a[i+3][j] * x[j];
				sigma3 += a[i+3][j+1] * x[j+1];
				sigma3 += a[i+3][j+2] * x[j+2];
				sigma3 += a[i+3][j+3] * x[j+3];
			}

            // Elementos faltantes que no se han podido recorrer en el bucle previo
            for(int j = total_mult8; j < n; j++){
                sigma0 += a[i][j] * x[j];
                sigma1 += a[i+1][j] * x[j];
                sigma2 += a[i+2][j] * x[j];
                sigma3 += a[i+3][j] * x[j];
            }

			// Ahora debemos eliminar la contribución del elemento diagonal
            // Esto se debe a que se ha suprimido el if(i!=j) del bucle previo para reducir instrucciones
			sigma0 -= a[i][i] * x[i];
			sigma1 -= a[i+1][i+1] * x[i+1];
			sigma2 -= a[i+2][i+2] * x[i+2];
			sigma3 -= a[i+3][i+3] * x[i+3];

			// Calculamos el nuevo elemento de x
			x_new[i] = (b[i] - sigma0) / a[i][i];
			x_new[i+1] = (b[i+1] - sigma1) / a[i+1][i+1];
			x_new[i+2] = (b[i+2] - sigma2) / a[i+2][i+2];
			x_new[i+3] = (b[i+3] - sigma3) / a[i+3][i+3];
			
			// Comprobamos la diferencia al cuadrado entre soluciones
			resta = x_new[i] - x[i];
			norm2 += resta * resta;
			resta = x_new[i+1] - x[i+1];
			norm2 += resta * resta;
			resta = x_new[i+2] - x[i+2];
			norm2 += resta * resta;
			resta = x_new[i+3] - x[i+3];
			norm2 += resta * resta;
		}

		// Bucle 2
		// Iteramos hasta el múltiplo de 8 más próximo a n, manteniéndose menor que este
		for(int i = total_mult8/2; i < total_mult8; i+=4){
			// Reiniciamos valor de sigma
			sigma0 = 0.0;
			sigma1 = 0.0;
			sigma2 = 0.0;
			sigma3 = 0.0;

			// Subbucle para incrementar sigma
			for(int j = 0; j < total_mult8; j+=4){
				// Desenrrollamos el lazo
				sigma0 += a[i][j] * x[j];
				sigma0 += a[i][j+1] * x[j+1];
				sigma0 += a[i][j+2] * x[j+2];
				sigma0 += a[i][j+3] * x[j+3];

				sigma1 += a[i+1][j] * x[j];
				sigma1 += a[i+1][j+1] * x[j+1];
				sigma1 += a[i+1][j+2] * x[j+2];
				sigma1 += a[i+1][j+3] * x[j+3];
				
				sigma2 += a[i+2][j] * x[j];
				sigma2 += a[i+2][j+1] * x[j+1];
				sigma2 += a[i+2][j+2] * x[j+2];
				sigma2 += a[i+2][j+3] * x[j+3];

				sigma3 += a[i+3][j] * x[j];
				sigma3 += a[i+3][j+1] * x[j+1];
				sigma3 += a[i+3][j+2] * x[j+2];
				sigma3 += a[i+3][j+3] * x[j+3];
			}

            // Elementos faltantes que no se han podido recorrer en el bucle previo
            for(int j = total_mult8; j < n; j++){
                sigma0 += a[i][j] * x[j];
                sigma1 += a[i+1][j] * x[j];
                sigma2 += a[i+2][j] * x[j];
                sigma3 += a[i+3][j] * x[j];
            }

			// Como eliminamos la línea if(i != j),
			// ahora debemos eliminar la contribución del elemento diagonal
			sigma0 -= a[i][i] * x[i];
			sigma1 -= a[i+1][i+1] * x[i+1];
			sigma2 -= a[i+2][i+2] * x[i+2];
			sigma3 -= a[i+3][i+3] * x[i+3];

			// Calculamos el nuevo elemento de x
			x_new[i] = (b[i] - sigma0) / a[i][i];
			x_new[i+1] = (b[i+1] - sigma1) / a[i+1][i+1];
			x_new[i+2] = (b[i+2] - sigma2) / a[i+2][i+2];
			x_new[i+3] = (b[i+3] - sigma3) / a[i+3][i+3];
			
			// Comprobamos la diferencia al cuadrado entre soluciones
			resta = x_new[i] - x[i];
			norm2 += resta * resta;
			resta = x_new[i+1] - x[i+1];
			norm2 += resta * resta;
			resta = x_new[i+2] - x[i+2];
			norm2 += resta * resta;
			resta = x_new[i+3] - x[i+3];
			norm2 += resta * resta;
		}

        // Elementos finales (en caso de que n no sea múltiplo de 8)
        for(int i = total_mult8; i < n; i++){
            sigma0 = 0.0;       // Rescatamos las variables usadas desde antes

            // Bucle análogo al anterior
            for (int j = 0; j < n; j++) {
                sigma0 += a[i][j] * x[j];
            }
            // Eliminamos la contribución del elemento diagonal
            sigma0 -= a[i][i] * x[i];

            // Calculamos el nuevo elemento de x
            x_new[i] = (b[i] - sigma0) / a[i][i];

            // Incrementamos el valor de la norma total
            resta = x_new[i] - x[i];
            norm2 += resta * resta;
        }

		// Actualizamos x
		for(int i = 0; i < total_mult8; i+=4){
			x[i] = x_new[i];
			x[i+1] = x_new[i+1];
			x[i+2] = x_new[i+2];
			x[i+3] = x_new[i+3];
		}

        // Añadimos los elementos restantes (si n no es múltiplo de 8)
        for(int i = total_mult8; i < n; i++){
            x[i] = x_new[i];
        }
		
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
	// Número de hilos (en este programa es solo 1)
	printf("\nv2 \t %d \t %.15f \t %lf \t 1 \n", n, norm2, ciclos);
	
	/* Liberar variables */
	for(int i = 0; i < n; i++) free(a[i]);
	free(a);
	free(b);
	free(x);
	free(x_new);

	return 0;
}

// COMPILACIÓN: gcc -o v2 v2.c -lm
// FORMATO DE EJECUCIÓN: ./v1 <tamaño vector> <número hilos> 
// Implementamos división de lazos y desenrrollamiento
// No es posible "reducir" el número de instrucciones:
//      Se elimina el if(i!=j) para evitar sumar elementos diagonales
//      Pero se añaden instrucciones adicionales para reducir los sigmas