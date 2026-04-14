#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "counter.h"
/* Librería para que el compilador seleccione las cabeceras de AVX256 */
#include <immintrin.h>

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
	double ciclos;	    // Número de ciclos medidos
	double norm2;	    // Norma del vector al cuadrado
    double *temporal;   // Array auxiliar para tratar de forma individual con los registros vectoriales
	int n = 0;	        // tamaño de los vectores

    // Alinearemos la memoria de temporal para poder utilizarla con las operaciones vectoriales
    temporal = (double*) aligned_alloc(64, 4 * sizeof(double));
	
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

    /* Cálculo de los índices para recorrer los bucles de 4 en 4 (análogo a v2)*/
    double total_mult4;
    // Primero, recorremos un múltiplo de 4 elementos de los vectores (para bucles con saltos de 4 en 4)
    // Posteriormente, los últimos 1 a 3 datos se recorren aparte (no será posible aplicar las operaciones vectoriales sobre estos)
    total_mult4 = n - (n%4);

	/* Declaración de registros para aplicar las operaciones vectoriales */
	__m256d avect[4];	    // 4 registros de 4 doubles cada uno para la matriz a
	__m256d adiagonal;	    // Registro de 4 doubles para los elementos diagonales de a (a[i][i])
	__m256d bvect;		    // Registro de 4 doubles para el vector b
	__m256d xvect;		    // Registro de 4 doubles para el vector x
	__m256d xnewvect;	    // Registro de 4 doubles para el vector x_new
	__m256d sigmavect[4];	// 4 registros de 4 doubles para los sigmas necesarios
	__m256d auxiliar;	    // Registro de 4 doubles para datos intermedios auxiliares

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

		// Bucle donde operaremos con los registros
		for(int i = 0; i < total_mult4; i+=4){
            // IMPORTANTE: usando set, los datos se almacenan en orden inverso (ie, 1 2 3 4 se almacenaría como 4 3 2 1)
			// Reiniciamos valor de sigma usando la operación de inicialización
			sigmavect[0] = _mm256_set_pd(0.0, 0.0, 0.0, 0.0);   // Para la fila i de a
            sigmavect[1] = _mm256_set_pd(0.0, 0.0, 0.0, 0.0);   // Para la fila i+1 de a
            sigmavect[2] = _mm256_set_pd(0.0, 0.0, 0.0, 0.0);   // Para la fila i+2 de a
            sigmavect[3] = _mm256_set_pd(0.0, 0.0, 0.0, 0.0);   // Para la fila i+3 de a

            // Cargamos los valores de b
            bvect = _mm256_set_pd(
                b[i+3],
                b[i+2],
                b[i+1],
                b[i]
            );
            // Cargamos los elementos diagonales de a
            adiagonal = _mm256_set_pd(
                a[i+3][i+3],
                a[i+2][i+2],
                a[i+1][i+1],
                a[i][i]
            );

			// Subbucle para incrementar sigma
			for(int j = 0; j < total_mult4; j+=4){
				/* Inicializamos los registros de la matriz a y el vector x */
				// _mm256d_set_pd permite añadir los doubles al registro definido
				avect[0] = _mm256_set_pd(
                    a[i][j+3],
                    a[i][j+2],
                    a[i][j+1],
                    a[i][j]
                );
				avect[1] = _mm256_set_pd(
                    a[i+1][j+3],
                    a[i+1][j+2],
                    a[i+1][j+1],
                    a[i+1][j]
                );
				avect[2] = _mm256_set_pd(
                    a[i+2][j+3],
                    a[i+2][j+2],
                    a[i+2][j+1],
                    a[i+2][j]
                );
				avect[3] = _mm256_set_pd(
                    a[i+3][j+3],
                    a[i+3][j+2],
                    a[i+3][j+1],
                    a[i+3][j]
                );
				xvect = _mm256_set_pd(
                    x[j+3],
                    x[j+2],
                    x[j+1],
                    x[j]
                );
				
                // Hacemos el cálculo sigma += a[i][j] * x[j], con los índices correspondientes
                // fmadd permite hacer el cálculo a*b + c, lo que ahorra 1 instrucción (en vez de multiplicar y luego sumar)
                sigmavect[0] = _mm256_fmadd_pd(avect[0], xvect, sigmavect[0]);
                sigmavect[1] = _mm256_fmadd_pd(avect[1], xvect, sigmavect[1]);
                sigmavect[2] = _mm256_fmadd_pd(avect[2], xvect, sigmavect[2]);
                sigmavect[3] = _mm256_fmadd_pd(avect[3], xvect, sigmavect[3]);
			}

            /* Sumamos los elementos de cada registro sigmavect para completar la suma */
            // hadd(a,b) pone en el registro destino el siguiente cálculo: (a[0] + a[1], b[0] + b[1], a[2] + a[3], b[2] + b[3])
            // Podemos aprovechar para sumar los sigmas de 2 en 2
            auxiliar = _mm256_hadd_pd (sigmavect[0], sigmavect[1]);
            // Trasladamos el resultado a un vector de doubles
            // IMPORTANTE: a diferencia de set, store sí que almacena los datos en el orden habitual
            _mm256_store_pd(temporal, auxiliar);
            // Sumamos los sigmas de la fila i y la i+1
            temporal[0] += temporal[2];
            temporal[1] += temporal[3];
            // Devolvemos a uno de los registros que ya no usaremos las sumas
            sigmavect[0] = _mm256_set_pd(
                0.0,
                0.0,
                temporal[1],
                temporal[0]
            );

            // Repetimos el procedimiento con los 2 sigmas faltantes
            auxiliar = _mm256_hadd_pd (sigmavect[2], sigmavect[3]);
            _mm256_store_pd(temporal, auxiliar);
            // Sumamos los sigmas de la fila i+2 y la i+3
            temporal[0] += temporal[2];
            temporal[1] += temporal[3];
            sigmavect[1] = _mm256_set_pd(
                temporal[1],
                temporal[0],
                0.0,
                0.0
            );

            // Obtenemos (en orden) los 4 sigmas pertinentes
            auxiliar = _mm256_add_pd(sigmavect[0], sigmavect[1]);

            /* Cálculos con los elementos faltantes (si n no es múltiplo de 4 y queda 1 bloque con menos de 4 datos) */
            /*
            * En este caso no será posible emplear operaciones vectoriales, a menos que se compruebe con if cuántos
            * datos quedan por encima de total_mult4, para así utilizar _mm256_set_pd con 0.0 en los lugares sin dato
            * Sin embargo, esta estrategia trae consigo una sobrecarga y una complicación del código (por añadir varios condicionales)
            */
            _mm256_store_pd(temporal, auxiliar);
            for(int j = total_mult4; j < n; j++){
                // Actualizamos cada valor de sigma
                temporal[0] += a[i][j] * x[j];
                temporal[1] += a[i+1][j] * x[j];
                temporal[2] += a[i+2][j] * x[j];
                temporal[3] += a[i+3][j] * x[j];
            }
            // Devolvemos los valores al registro
            auxiliar = _mm256_set_pd(temporal[3], temporal[2], temporal[1], temporal[0]);

			/* Eliminamos la contribución del elemento diagonal (ie, sigma -= a[i][i] * x[i]) */
			// Multiplicamos los elementos diagonales a[i][i] por x[i]
            xvect = _mm256_set_pd(
                    x[i+3],
                    x[i+2],
                    x[i+1],
                    x[i]
                );
            // fnmadd permite permite hacer el producto a[i][i] * x[i] y directamente restárselo a sigma
			auxiliar = _mm256_fnmadd_pd(adiagonal, xvect, auxiliar);
			
			/* Calculamos el nuevo elemento de x (x_new[i] = (b[i] - sigma) / a[i][i]) */
			// Hacemos la resta b[i] - sigma[i]
			auxiliar = _mm256_sub_pd(bvect, auxiliar);
			// Dividimos el valor auxiliar anterior entre las diagonales de a
			xnewvect = _mm256_div_pd(auxiliar, adiagonal);

            // Devolvemos al array original x_new los datos calculados en el registro correspondiente xnewvect
            // Como vamos a almacenar 4 datos en x_new[i], ..., x_new[i+3], podemos emplear el puntero directamente, sin usar variables auxiliares
            _mm256_store_pd(x_new + i, xnewvect);
			
			/* Comprobamos la diferencia entre soluciones x y x_new (resta = x_new[i] - x[i]) */
			// Calculamos x_new[i] - x[i] y lo elevamos al cuadrado
			auxiliar = _mm256_sub_pd(xnewvect, xvect);
            auxiliar = _mm256_mul_pd(auxiliar, auxiliar);
			// Almacenamos los doubles del registro auxiliar en un vector para calcular individualmente
			_mm256_store_pd(temporal, auxiliar);
			// Sumamos las diferencias entre x_new y x elevadas al cuadrado a la norma (norm2 += resta * resta)
			norm2 += temporal[0];
			norm2 += temporal[1];
			norm2 += temporal[2];
			norm2 += temporal[3];
		}

        /* Cálculos con los elementos faltantes (si n no es múltiplo de 4) */
        // En este caso no será posible emplear los registros de 4 elementos, al haber menos (1, 2 o 3)
        for(int i = total_mult4; i < n; i++){
            // Aprovechamos el array temporal para emplearlo como el nuevo sigma
            temporal[0] = 0.0;

            // Sumamos las contribuciones de los elementos pertinentes
            for (int j = 0; j < n; j++) {
                temporal[0] += a[i][j] * x[j];
            }
            // Eliminamos la contribución del elemento diagonal
            temporal[0] -= a[i][i] * x[i];

            // Calculamos el nuevo elemento de x
            x_new[i] = (b[i] - temporal[0]) / a[i][i];

            // Incrementamos el valor de la norma total
            // Reutilizamos temporal[0] de nuevo porque no se volverá a emplear
            temporal[0] = x_new[i] - x[i];
            norm2 += temporal[0] * temporal[0];
        }
        
		// Actualizamos el array x
        /*
        * No resulta óptimo, en este caso, tratar con los registros vectoriales por el medio, pues traerían una sobrecarga
        * demasiado alta e instrucciones adicionales sin sentido para pasar de registros a arrays y viceversa
        */
		for(int i = 0; i < n; i++){
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
	printf("\nv4 \t %d \t %.15f \t %lf \t 1 \n", n, norm2, ciclos);
	
	/* Liberar variables */
    free(temporal);
	for(int i = 0; i < n; i++) free(a[i]);
	free(a);
	free(b);
	free(x);
	free(x_new);

	return 0;
}

// COMPILACIÓN: gcc -o v4 v4.c -lm -mavx2 -mfma