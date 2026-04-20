/* ============================================================
 * prod_cons_thread.c  –  Productor-Consumidor con threads POSIX
 * Ejercicio 1: problema del productor-consumidor con threads
 *
 * Diferencias principales respecto a la versión con procesos
 * (prod_sem.c / cons_sem.c):
 *
 *   1. Un único programa lanza DOS THREADS con pthread_create():
 *      uno actuará de productor y otro de consumidor.
 *   2. Los threads comparten el espacio de memoria del proceso,
 *      por lo que el buffer y los contadores de vocales son
 *      simplemente variables globales. NO se necesita mmap().
 *   3. Los semáforos son ANÓNIMOS (sem_init / sem_destroy),
 *      sin nombre en el sistema de ficheros. NO se necesita
 *      sem_open / sem_unlink.
 *   4. El hilo principal espera a que ambos threads terminen
 *      con pthread_join() antes de imprimir resultados y salir.
 *
 * Comportamiento de velocidades (mismo criterio que apartado 2):
 *   - Iteraciones  0-29:  productor rápido (sleep 0 s),
 *                          consumidor lento  (sleep 2 s) → buffer lleno
 *   - Iteraciones 30-59:  productor lento  (sleep 2 s),
 *                          consumidor rápido (sleep 0 s) → buffer vacío
 *   - Iteraciones 60-79:  ambos con sleep aleatorio 0-3 s
 *
 * Compilar con:  gcc -o prod_cons_thread prod_cons_thread.c -pthread
 * Ejecutar con:  ./prod_cons_thread
 * ============================================================ */

#include <stdio.h>      /* printf, fopen, fgetc, fclose              */
#include <stdlib.h>     /* exit, srand, rand                         */
#include <pthread.h>    /* pthread_t, pthread_create, pthread_join   */
#include <unistd.h>     /* sleep                                     */
#include <time.h>       /* time() para semilla aleatoria             */

/* ============================================================
 * Constantes globales
 * ============================================================ */

/* Tamaño del buffer compartido (pila FIFO) */
#define N         10

/* Número máximo de iteraciones del bucle principal de cada thread */
#define MAX_ITER  80

/* ============================================================
 * Variables globales compartidas por ambos threads
 * Al estar en el mismo proceso no hace falta mmap():
 * los threads ven directamente las variables del proceso.
 * ============================================================ */
/* Variable global para el mutex */
pthread_mutex_t el_mutex;

/* Variables de condición para consumidor y productor, respectivamente */
pthread_cond_t condc, condp;

/* Buffer compartido tipo pila FIFO de tamaño N */
int buffer[N];

/* Suma de enteros realizada por el PRODUCTOR */
int suma_prod;

/* Suma de enteros realizada por el CONSUMIDOR */
int suma_cons;

/* Índice para indicar la posición en el buffer del último entero eliminado */
int siguiente_posicion;

/* ============================================================
 * Funciones auxiliares compartidas
 * ============================================================ */

/* ------------------------------------------------------------
 * produce_item()
 *   Lee el siguiente entero del archivo y lo suma a la variable global.
 *   Debe llamarse SIEMPRE dentro de la sección crítica.
 *
 *   Parámetro:
 *     *archivo – puntero al archivo donde se leerá el entero
 *
 *   Retorna:
 *     El entero leido.
 * ------------------------------------------------------------ */
int produce_item(FILE *archivo){
	int entero, finarchivo;

	finarchivo = fscanf(archivo,"%d", &entero); /* Leemos el siguiente entero del archivo y actualiza el puntero */
	if(finarchivo != 1) return 101;   /* Usamos el entero especial 101 para indicar que se ha terminado de leer el archivo */

	suma_prod += entero;		/* Sumamos el entero leido a la variable global del productor    */

	printf("[PROD] Producido el item %d\n", entero);

	return entero;			/* Devolvemos el entero leido para insertarlo en el buffer       */
}

/* ------------------------------------------------------------
 * insert_item()
 *   Inserta un entero en la primera posición libre del buffer.
 *   Hay que empezar por la última posición en la que se eliminó un elemento.
 *   Debe llamarse SIEMPRE dentro de la sección crítica.
 *
 *   Parámetro:
 *     item – entero a insertar en el buffer compartido
 * ------------------------------------------------------------ */
void insert_item(int item){
    for(int i = siguiente_posicion; i < N; i++){      /* recorremos desde la última posición liberada    */
        if((buffer[i] == 100) || (buffer[i] == 0)){   /* busca la primera posición libre (un 0)          */
            buffer[i] = item;                         /* escribe el entero en esa posición               */
            return;                                   /* inserta sólo una vez                            */
        }
    }
    for(int i = 0; i < siguiente_posicion; i++){      /* Recorremos las primeras posiciones no visitadas */
        if((buffer[i] == 100) || (buffer[i] == 0)){   /* busca la primera posición libre (un 0)          */
            buffer[i] = item;                         /* escribe el entero en esa posición               */
            return;                                   /* inserta sólo una vez                            */
        }
    }
}

/* ------------------------------------------------------------
 * remove_item()
 *   Retira el elemento del tope de la pila FIFO (el primero no
 *   vacío desde el último retirado) y marca la posición como libre con 0.
 *   Debe llamarse SIEMPRE dentro de la sección crítica.
 *
 *   Parámetros:
 *     cond1 – se pone a 1 si el elemento retirado es 101 (EOF)
 *     cond2 – se pone a 1 si la pila queda completamente vacía
 *
 *   Retorna:
 *     El entero retirado, o 100 si el buffer estaba vacío.
 * ------------------------------------------------------------ */
int remove_item(int *cond1, int *cond2){
    for(int i = siguiente_posicion; i < N; i++){       /* recorre de la última posición con hueco hasta N*/
        if((buffer[i] != 0) && (buffer[i] != 100)){    /* busca el primer elemento válido         */

            if(buffer[i] == 101){                      /* token de fin de fichero del productor   */
                *cond1 = 1;                            /* activa bandera de EOF detectado         */
            }

            int numero = buffer[i];                     /* guarda el carácter a devolver           */
            buffer[i] = 0;                              /* marca la posición como libre (consumida)*/
	    siguiente_posicion = ((i+1 >= N) ? 0 : i+1); /* actualizamos la siguiente posición 
	    						 * (evitando salirnos del rango 0 a N     */
            return numero;                              /* devuelve el carácter retirado           */
        }
    }
    for(int i = 0; i < siguiente_posicion; i++){       /* recorre los índices que faltan          */
        if((buffer[i] != 0) && (buffer[i] != 100)){    /* busca el primer elemento válido         */

            if(buffer[i] == 101){                      /* token de fin de fichero del productor   */
                *cond1 = 1;                            /* activa bandera de EOF detectado         */
            }

            int numero = buffer[i];                     /* guarda el carácter a devolver           */
            buffer[i] = 0;                              /* marca la posición como libre (consumida)*/
	    siguiente_posicion = ((i+1 >= N) ? 0 : i+1); /* actualizamos la siguiente posición 
	    						 * (evitando salirnos del rango 0 a N     */
            return numero;                              /* devuelve el carácter retirado           */
        }
    }

    *cond2 = 1; /* activa bandera de fin de pila FIFO      */
    return 0;   /* buffer completamente vacío */
}

/* ------------------------------------------------------------
 * consume_item()
 *   Añadimos la contribución del item a la suma total de elementos contados por el consumidor
 * ------------------------------------------------------------ */
void consume_item(int item){
	suma_cons += item;

	printf("[CONS] Consumido el item %d\n", item);

	return;
}

/* ------------------------------------------------------------
 * sleep_productor()
 *   Aplica el retardo del productor FUERA de la sección crítica.
 *     iter  0-29: 0 s (productor rápido)
 *     iter 30-59: 2 s (productor lento)
 *     iter 60-79: aleatorio 0-3 s
 * ------------------------------------------------------------ */
void sleep_productor(int iter){
    if(iter == 30) printf("\n[PROD] Iteración 30 alcanzada. Productor más lento ahora.\n\n");
    if(iter == 60) printf("\n[PROD] Iteración 60 alcanzada. Durmiendo aleatoriamente.\n\n");
    if(iter < 30){
        sleep(0);           /* productor más rápido que el consumidor */
    } else if(iter < 60){
        sleep(2);           /* productor más lento que el consumidor  */
    } else {
        sleep(rand() % 4);  /* tiempo aleatorio entre 0 y 3 segundos  */
    }
}

/* ------------------------------------------------------------
 * sleep_consumidor()
 *   Aplica el retardo del consumidor FUERA de la sección crítica.
 *     iter  0-29: 2 s (consumidor lento,  buffer se llena)
 *     iter 30-59: 0 s (consumidor rápido, buffer se vacía)
 *     iter 60-79: aleatorio 0-3 s
 * ------------------------------------------------------------ */
void sleep_consumidor(int iter){
    if(iter == 30) printf("\n[CONS] Iteración 30 alcanzada. Consumidor más rápido ahora.\n\n");
    if(iter == 60) printf("\n[CONS] Iteración 60 alcanzada. Durmiendo aleatoriamente.\n\n");
    if(iter < 30){
        sleep(2);           /* consumidor más lento que el productor  */
    } else if(iter < 60){
        sleep(0);           /* consumidor más rápido que el productor */
    } else {
        sleep(rand() % 4);  /* tiempo aleatorio entre 0 y 3 segundos  */
    }
}

/* ------------------------------------------------------------
 * buffer_lleno()
 *   Se hace una espera activa hasta que se encuentre algún 0 en el buffer.
 *   Devuelve 1 si está lleno y 0 si no lo está
 * ------------------------------------------------------------ */
int buffer_lleno(){
	int i = 0;
	while(buffer[i] != 0){   /* Solo saldremos del bucle al encontrar un hueco en el buffer */
		i++;
		if(i >= N) return 1; /* Buffer completamente lleno */
	}
	return 0; /* Buffer no lleno */
}

/* ------------------------------------------------------------
 * buffer_vacio()
 *   Se hace una espera activa hasta que se encuentre algún elemento distinto de 0 en el buffer.
 *   Devuelve 0 si está completamente vacío y 1 si no lo está
 * ------------------------------------------------------------ */
int buffer_vacio(){
	int i = 0;
	while(buffer[i] == 0){   /* Solo saldremos del bucle al encontrar un hueco en el buffer */
		i++;
		if(i >= N) return 0; /* Buffer completamente vacío */
	}
	return 1; /* Buffer tiene al menos 1 elemento */
}

/* ============================================================
 * Función del thread PRODUCTOR
 * ============================================================
 * Prototipo obligatorio de pthread_create(): void* func(void*)
 *
 * Lógica:
 *   1. Abre el fichero de texto de entrada.
 *   2. Lee MAX_ITER enteros (o hasta EOF).
 *   3. Para cada entero, aplica el protocolo de productor.
 *   4. Inserta 101 como token de fin de fichero.
 *   5. Cierra el fichero y termina el thread.
 * ============================================================ */
void *hilo_productor(void *arg){
    /* arg no se usa; el fichero y el buffer son variables globales */
    (void)arg;

    int iter = 0;   /* contador de iteraciones del bucle del productor */

    /* Abre el fichero de texto de entrada en modo lectura */
    FILE *archivo = fopen("archivo.txt", "r");
    if(archivo == NULL){
        printf("[PROD] Error: no se puede abrir archivo.txt\n");
        pthread_exit(NULL); /* termina el thread si no existe el fichero */
    }

    printf("[PROD] Thread iniciado. Leyendo archivo.txt...\n");

    /* -----------------------------------------------------------
     * Bucle principal del productor:
     * Lee un entero por iteración hasta EOF o MAX_ITER.
     * ----------------------------------------------------------- */
    int entero = 1; /* entero leído del fichero (int para comparar con EOF)
    		     * se inicializa a 1 para que sea distinto a EOF y podamos pasar al bucle.
		     * Después de cumplir la condición del while, se sobreescribe con produce_item */

    for(int iter = 0; iter < MAX_ITER; iter++){
	/* Salimos del bucle en caso de terminar de leer el archivo */
	if(entero == 101){
		/* Despertamos al consumidor y desbloqueamos el mutex antes de salir */
		pthread_cond_signal(&condp);
		pthread_mutex_unlock(&el_mutex);
		break;
	}

	/* Acceso exclusivo al buffer */
	pthread_mutex_lock(&el_mutex);

	/* Mientras el buffer está lleno (la función devuelve 1) esperamos */
	while(buffer_lleno() == 1) pthread_cond_wait(&condp, &el_mutex);

	/* === SECCIÓN CRÍTICA ======================================= */
        entero = produce_item(archivo); /* Leer el un entero del archivo y se suma a la variable global */

	/* Aquí se pueden producir las carreras críticas    */
	insert_item(entero); /* inserta el entero en el buffer compartido   */
   	/* =========================================================== */

	/* Despertamos al consumidor, porque existe un item en el buffer para consumir */
	pthread_cond_signal(&condc);

	/* Liberamos acceso al buffer */
	pthread_mutex_unlock(&el_mutex);

	/* Retardo de velocidad fuera de la región crítica */
        sleep_productor(iter); /* ajusta la velocidad según la fase de la iteración */
    }

    /* -------------------------------------------------------
     * Inserta el token 101 de fin de fichero para que el
     * consumidor sepa que no llegarán más datos.
     * Debemos repetir el procedimiento de inserción con las
     * variables de condición y el mutex.
     * ------------------------------------------------------- */
    pthread_mutex_lock(&el_mutex);

    while(buffer_lleno() == 1) pthread_cond_wait(&condp, &el_mutex);

    insert_item(101); /* inserta el marcador de fin de archivo */

    pthread_cond_signal(&condc);
    pthread_mutex_unlock(&el_mutex);
    
    fclose(archivo); /* cierra el fichero de texto de entrada        */

    printf("[PROD] Thread finalizado.\n");
    pthread_exit(NULL); /* termina el thread del productor correctamente */
}

/* ============================================================
 * Función del thread CONSUMIDOR
 * ============================================================
 * Prototipo obligatorio de pthread_create(): void* func(void*)
 *
 * Lógica:
 *   1. Espera a que haya datos con sem_wait(llenas).
 *   2. Accede en exclusión mutua al buffer con sem_wait(mutex).
 *   3. Retira el elemento del tope de la pila con remove_item().
 *   4. Sale de S.C. con sem_post(mutex) y señala hueco libre
 *      con sem_post(vacias).
 *   5. Clasifica el carácter retirado fuera de la S.C.
 *   6. Termina cuando detecta '#' y la pila queda vacía.
 * ============================================================ */
void *hilo_consumidor(void *arg){
    /* arg no se usa; el buffer es una variable global */
    (void)arg;

    int eof_detectado = 0; /* bandera: 1 cuando se retiró el token 101            */
    int fin_pila      = 0; /* bandera: 1 cuando la pila quedó completamente vacía */
    int iter = 0;          /* contador de iteraciones del bucle del consumidor    */

    printf("[CONS] Thread iniciado. Esperando datos...\n");

    /* -----------------------------------------------------------
     * Bucle principal del consumidor:
     * Continúa hasta que se ha detectado EOF y la pila está vacía,
     * o se alcanzan MAX_ITER iteraciones.
     * ----------------------------------------------------------- */
    for(int iter = 0; iter < MAX_ITER; iter++){
	/* Salimos al detectar EOF y en cuando la pila esté vacía */
	if(eof_detectado && fin_pila) break;

	/* Acceso exclusivo al buffer */
	pthread_mutex_lock(&el_mutex);

	/* Esperamos mientras el buffer está vacío */
	while(buffer_vacio() == 0) pthread_cond_wait(&condc, &el_mutex);

        /* === SECCIÓN CRÍTICA ======================================= */
	/* Se pueden producir carreras críticas al buscar items en las posiciones */
        int item = remove_item(&eof_detectado, &fin_pila); /* retira del buffer */
        /* === FIN SECCIÓN CRÍTICA =================================== */

	/* Salimos si leemos el final del archivo del buffer */
	if(item == 101) break;

        /* Sumamos el último item retirado del buffer */
        consume_item(item); /* consumimos el item */

   	/* Despertamos al productor */
	pthread_cond_signal(&condp);

	/* Liberamos el acceso al buffer */
	pthread_mutex_unlock(&el_mutex);
        
	/* Retardo de velocidad FUERA de la sección crítica */
        sleep_consumidor(iter); /* ajusta la velocidad según la fase de la iteración */
    }

    printf("[CONS] Thread finalizado.\n");
    pthread_exit(NULL); /* termina el thread del consumidor correctamente */
}

/* ============================================================
 * main()
 *   Hilo principal del programa.
 *   1. Inicializa el buffer y los semáforos anónimos.
 *   2. Lanza los dos threads (productor y consumidor).
 *   3. Espera a que ambos terminen con pthread_join().
 *   4. Imprime los recuentos de vocales de ambos threads.
 *   5. Destruye los semáforos con sem_destroy().
 * ============================================================ */
int main(void){

    srand(time(NULL)); /* inicializa la semilla aleatoria para los sleep aleatorios */

    /* -------------------------------------------------------
     * Inicialización del buffer compartido
     * Al ser variable global, ya está a '\0', pero se
     * reinicializa explícitamente para mayor claridad.
     * ------------------------------------------------------- */
    for(int i = 0; i < N; i++){
        buffer[i] = 0; /* marca cada celda del buffer como vacía al inicio */
    }

    /* -------------------------------------------------------
     * Inicialización de variables a cero
     * ------------------------------------------------------- */
    suma_prod = 0;
    suma_cons = 0;
    siguiente_posicion = 0;

    /* -------------------------------------------------------
     * Inicialización del mutex y de las variables de condición
     * ------------------------------------------------------- */
    pthread_mutex_init(&el_mutex, 0);

    pthread_cond_init(&condc, 0); /* Variable de condición del consumidor */
    pthread_cond_init(&condp, 0); /* Variable de condición del productor */

    /* -------------------------------------------------------
     * Creación de los dos threads con pthread_create()
     *
     * Prototipo: int pthread_create(pthread_t *thread,
     *                               const pthread_attr_t *attr,
     *                               void *(*start_routine)(void*),
     *                               void *arg);
     *   - thread: identificador del thread (salida)
     *   - attr:   NULL → atributos por defecto
     *   - start_routine: función que ejecutará el thread
     *   - arg:    argumento a pasar a la función (NULL aquí)
     * ------------------------------------------------------- */
    pthread_t tid_prod; /* identificador del thread productor  */
    pthread_t tid_cons; /* identificador del thread consumidor */

    /* Crea el thread productor; ejecutará hilo_productor() */
    if(pthread_create(&tid_prod, NULL, hilo_productor, NULL) != 0){
        perror("Error al crear thread productor"); /* mensaje de error del sistema */
        exit(1);
    }

    /* Crea el thread consumidor; ejecutará hilo_consumidor() */
    if(pthread_create(&tid_cons, NULL, hilo_consumidor, NULL) != 0){
        perror("Error al crear thread consumidor"); /* mensaje de error del sistema */
        exit(1);
    }


    /* -------------------------------------------------------
     * Espera a que ambos threads terminen con pthread_join()
     *
     * pthread_join() bloquea el hilo principal hasta que el
     * thread indicado termine (equivalente a waitpid() para
     * procesos). El segundo argumento (NULL) descarta el
     * valor de retorno del thread.
     * ------------------------------------------------------- */
    pthread_join(tid_prod, NULL); /* espera a que termine el thread productor  */
    pthread_join(tid_cons, NULL); /* espera a que termine el thread consumidor */

    printf("[MAIN] Ambos threads han terminado.\n\n");

    /* -------------------------------------------------------
     * Imprime los resultados de ambos threads
     * ------------------------------------------------------- */

    printf("Suma [PROD] = %d\n", suma_prod);
    printf("Suma [CONS] = %d\n", suma_cons);

    printf("Diferencia entre sumas = %d\n\n", suma_prod - suma_cons);

    /* -------------------------------------------------------
     * Destruimos el mutex y las variables de condición
     * ------------------------------------------------------- */
    pthread_cond_destroy(&condc);
    pthread_cond_destroy(&condp);

    pthread_mutex_destroy(&el_mutex);

    return 0;
}

//EJECUCION Y COMPILACION:

// gcc -o prod_cons_mutex prod_cons_mutex.c -pthread
// ./prod_cons_mutex
