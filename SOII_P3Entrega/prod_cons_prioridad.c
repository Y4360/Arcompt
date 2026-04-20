/* ============================================================
 * prod_cons_prioridad.c  –  Productor-Consumidor con threads POSIX
 * 
 * Compilar con:  gcc -o prod_cons_thread prod_cons_prioridad.c -pthread
 * Ejecutar con:  ./prod_cons_prioridad
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

/*
 * Estructura que contendrá los enteros del buffer con la prioridad
 * 1 para mayor prioridad y 3 para menor
 *
 */
typedef struct{
	int item;
	int prioridad;
}elemento_priorizado;

/* ============================================================
 * Variables globales compartidas por ambos threads
 * Al estar en el mismo proceso no hace falta mmap():
 * los threads ven directamente las variables del proceso.
 * ============================================================ */
/* Variable global para el mutex */
pthread_mutex_t el_mutex;

/* Variables de condición para consumidor y productor, respectivamente */
pthread_cond_t condc, condp1, condp2, condp3;

/* Buffer compartido tipo pila FIFO de tamaño N */
elemento_priorizado buffer[N];

/* Suma de enteros realizada por los PRODUCTORES */
int suma_prod1, suma_prod2, suma_prod3;

/* Sumas de enteros realizada por el CONSUMIDOR */
int suma_cons1, suma_cons2, suma_cons3;

/* Variable auxiliar para dotar al buffer de una estructura de pila FIFO */
int sig_pos;

/* ============================================================
 * Funciones auxiliares compartidas
 * ============================================================ */

/* ------------------------------------------------------------
 * produce_item()
 *   Lee el siguiente entero del archivo y lo suma a la variable global.
 *   Debe llamarse SIEMPRE dentro de la sección crítica.
 *
 *   Parámetro:
 *     *archivo – puntero al archivo donde se leerá el entero (será archivo1, 2 o 3 dependiendo del productor
 *     prioridad - entero que representa la prioridad en función del productor que invoque la función
 *
 *   Retorna:
 *     El entero leido con su prioridad.
 * ------------------------------------------------------------ */
elemento_priorizado produce_item(FILE *archivo, int prioridad){
	int entero, finarchivo;
	elemento_priorizado resultado;
	resultado.item = 101;
	resultado.prioridad = 0;

	finarchivo = fscanf(archivo,"%d", &entero); /* Leemos el siguiente entero del archivo y actualiza el puntero */
	if(finarchivo != 1) return resultado;   /* Usamos el entero especial 101 para indicar que se ha terminado de leer el archivo */

	/* Sumamos el item en función de la prioridad del archivo desde el que se lee */
	if(prioridad == 1) suma_prod1 += entero; /* Sumamos el entero leido a la variable global del productor    */
	if(prioridad == 2) suma_prod2 += entero; /* Sumamos el entero leido a la variable global del productor    */
	if(prioridad == 3) suma_prod3 += entero; /* Sumamos el entero leido a la variable global del productor    */

	printf("[PROD] Producido el item %d con prioridad %d\n", entero, prioridad);

	/* Construimos el item */
	resultado.item = entero;
	resultado.prioridad = prioridad;

	return resultado;		/* Devolvemos el entero leido para insertarlo en el buffer       */
}

/* ------------------------------------------------------------
 * insert_item()
 *   Inserta un entero y su prioridad en la primera posición libre del buffer.
 *   Hay que empezar por la última posición en la que se eliminó un elemento.
 *   Debe llamarse SIEMPRE dentro de la sección crítica.
 *
 *   Parámetro:
 *     item – entero a insertar en el buffer compartido, junto con su prioridad
 * ------------------------------------------------------------ */
void insert_item(elemento_priorizado item){
    for(int i = sig_pos; i < N+sig_pos; i++){      /* recorremos desde la última posición liberada    */
        if((buffer[i%N].item == 100) || (buffer[i%N].item == 0)){   /* busca la primera posición libre (un 0)          */
            buffer[i%N] = item;                         /* escribe el elemento en esa posición               */

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
 *   Retorna:
 *     El entero retirado, o 0 si el buffer estaba vacío.
 * ------------------------------------------------------------ */
elemento_priorizado remove_item(){
    elemento_priorizado item;
    item.item = 0;
    item.prioridad = 0;

    /* Buscamos los elementos de cada prioridad (empezando por la última posición de la que se retiró un item) */
    for (int k=sig_pos; k<N+sig_pos; k++){
	if(buffer[k%N].item!=0 && buffer[k%N].prioridad == 1){
		item = buffer[k%N];

		buffer[k%N].item=0;
		buffer[k%N].prioridad=0; /* Al poner prioridad 0 no consideraremos el elemento en los siguientes bucles */

		sig_pos = (k+1)%N;	 /* Actualizamos el puntero de comienzo de los bucles de retirada de items      */
		
		return item;
	}
    }
    for (int k=sig_pos; k<N+sig_pos; k++){
	if(buffer[k%N].item!=0 && buffer[k%N].prioridad == 2){
		item=buffer[k%N];

		buffer[k%N].item=0;
		buffer[k%N].prioridad=0;

		sig_pos = (k+1)%N;

		return item;
	}
    }
    for (int k=sig_pos; k<N+sig_pos; k++){
	if(buffer[k%N].item!=0 && buffer[k%N].prioridad == 3){
		item=buffer[k%N];

		buffer[k%N].item=0;
		buffer[k%N].prioridad=0;

		sig_pos = (k+1)%N;

		return item;
	}
    }

    /* Si no encontramos elementos con prioridad */
    /* Llegamos a este punto del código si no se ha ejecutado ninguno de los 3 return anteriores */
    item.item = 0;
    item.prioridad = 0;

    return item;          /* devuelve el carácter retirado           */ 
}

/* ------------------------------------------------------------
 * Funciones de sueño
 * 	Productor y consumidor dormirán valores aleatorios en rangos diferentes
 * ------------------------------------------------------------ */
void sleep_productor(){
    sleep(rand() % 7);  /* tiempo aleatorio entre 0 y 6 segundos  */
}
void sleep_consumidor(){
    sleep(rand() % 4);  /* tiempo aleatorio entre 0 y 3 segundos  */
}

/* ------------------------------------------------------------
 * consume_item()
 *   Añadimos la contribución del item a la suma total de elementos contados por el consumidor
 * ------------------------------------------------------------ */
void consume_item(elemento_priorizado item){
	if(item.item < 100){
		if(item.prioridad == 1) suma_cons1 += item.item;
		if(item.prioridad == 2) suma_cons2 += item.item;
		if(item.prioridad == 3) suma_cons3 += item.item;
	}

	if(item.item != 0) printf("\t[CONS] Consumido el item %d con prioridad %d\n", item.item, item.prioridad);

	return;
}

/* ------------------------------------------------------------
 * buffer_lleno()
 *   Se hace una espera activa hasta que se encuentre algún 0 en el buffer.
 *   Devuelve 1 si está lleno y 0 si no lo está
 * ------------------------------------------------------------ */
int buffer_lleno(){
	int i = 0;
	while(buffer[i].item != 0){   /* Solo saldremos del bucle al encontrar un hueco en el buffer */
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
	for(int i=0; i<N; i++){
		/* No consideraremos el entero para EOF (101) como un elemento válido que ocupe un hueco */
		if(buffer[i].item != 0 && buffer[i].item != 101) return 1;
	}
	return 0; /* Buffer no tiene ningún elemento */
}

/* ============================================================
 * Función del thread PRODUCTOR1
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
void *hilo_productor1(void *arg){
    /* arg no se usa; el fichero y el buffer son variables globales */
    (void)arg;

    /* Abre el fichero de texto de entrada en modo lectura */
    FILE *archivo = fopen("archivo1.txt", "r");
    if(archivo == NULL){
        printf("[PROD1] Error: no se puede abrir archivo.txt\n");
        pthread_exit(NULL); /* termina el thread si no existe el fichero */
    }

    printf("[PROD1] Thread iniciado. Leyendo archivo.txt...\n");

    /* -----------------------------------------------------------
     * Bucle principal del productor:
     * Lee un entero por iteración hasta EOF o MAX_ITER.
     * ----------------------------------------------------------- */
    elemento_priorizado item;
    item.item = 1; /* entero leído del fichero (int para comparar con EOF)
    		     * se inicializa a 1 para que sea distinto a EOF y podamos pasar al bucle.
		     * Después de cumplir la condición del while, se sobreescribe con produce_item */
    item.prioridad = 1;

    for(int iter = 0; iter < MAX_ITER; iter++){
	/* Salimos del bucle en caso de terminar de leer el archivo */
	if(item.item == 101) break;
        
	/* Acceso exclusivo al buffer */
	pthread_mutex_lock(&el_mutex);

	/* Mientras el buffer está lleno (la función devuelve 1) esperamos */
	while(buffer_lleno() == 1) pthread_cond_wait(&condp1, &el_mutex);

	/* === SECCIÓN CRÍTICA ======================================= */
	/* En este caso, producimos un item con prioridad máxima */
        item = produce_item(archivo,1); /* Leer el un entero del archivo y se suma a la variable global */

	/* Aquí se pueden producir las carreras críticas    */
	insert_item(item); /* inserta el entero en el buffer compartido   */
   	/* =========================================================== */

	/* Despertamos al consumidor, porque existe un item en el buffer para consumir */
	pthread_cond_signal(&condc);

	/* Liberamos acceso al buffer */
	pthread_mutex_unlock(&el_mutex);

	/* Retardo de velocidad fuera de la región crítica */
        sleep_productor(); 
    }

    /* -------------------------------------------------------
     * Inserta el token 101 de fin de fichero para que el
     * consumidor sepa que no llegarán más datos.
     * Debemos repetir el procedimiento de inserción con las
     * variables de condición y el mutex.
     * ------------------------------------------------------- */
    pthread_mutex_lock(&el_mutex);

    while(buffer_lleno() == 1) pthread_cond_wait(&condp1, &el_mutex);

    item.item = 101;
    item.prioridad = 1;
    insert_item(item); /* inserta el marcador de fin de archivo */

    /* Despertamos todos los hilos que pueden estar bloqueados por condp */
    pthread_cond_broadcast(&condc);
    pthread_mutex_unlock(&el_mutex);

    fclose(archivo); /* cierra el fichero de texto de entrada        */

    printf("[PROD1] Thread finalizado.\n");
    pthread_exit(NULL); /* termina el thread del productor correctamente */
}

/* ============================================================
 * Función del thread PRODUCTOR2
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
void *hilo_productor2(void *arg){
    /* arg no se usa; el fichero y el buffer son variables globales */
    (void)arg;

    /* Abre el fichero de texto de entrada en modo lectura */
    FILE *archivo = fopen("archivo2.txt", "r");
    if(archivo == NULL){
        printf("[PROD2] Error: no se puede abrir archivo.txt\n");
        pthread_exit(NULL); /* termina el thread si no existe el fichero */
    }

    printf("[PROD2] Thread iniciado. Leyendo archivo.txt...\n");

    /* -----------------------------------------------------------
     * Bucle principal del productor:
     * Lee un entero por iteración hasta EOF o MAX_ITER.
     * ----------------------------------------------------------- */
    elemento_priorizado item;
    item.item = 1; /* entero leído del fichero (int para comparar con EOF)
    		     * se inicializa a 1 para que sea distinto a EOF y podamos pasar al bucle.
		     * Después de cumplir la condición del while, se sobreescribe con produce_item */
    item.prioridad = 2;

    for(int iter = 0; iter < MAX_ITER; iter++){
	/* Salimos del bucle en caso de terminar de leer el archivo */
	if(item.item == 101) break;
        
	/* Acceso exclusivo al buffer */
	pthread_mutex_lock(&el_mutex);

	/* Mientras el buffer está lleno (la función devuelve 1) esperamos */
	while(buffer_lleno() == 1) pthread_cond_wait(&condp2, &el_mutex);

	/* === SECCIÓN CRÍTICA ======================================= */
	/* En este caso, producimos un item con prioridad segunda */
        item = produce_item(archivo,2); /* Leer el un entero del archivo y se suma a la variable global */

	/* Aquí se pueden producir las carreras críticas    */
	insert_item(item); /* inserta el entero en el buffer compartido   */
   	/* =========================================================== */

	/* Despertamos al consumidor, porque existe un item en el buffer para consumir */
	pthread_cond_signal(&condc);

	/* Liberamos acceso al buffer */
	pthread_mutex_unlock(&el_mutex);

	/* Retardo de velocidad fuera de la región crítica */
        sleep_productor(); 
    }

    /* -------------------------------------------------------
     * Inserta el token 101 de fin de fichero para que el
     * consumidor sepa que no llegarán más datos.
     * Debemos repetir el procedimiento de inserción con las
     * variables de condición y el mutex.
     * ------------------------------------------------------- */
    pthread_mutex_lock(&el_mutex);

    while(buffer_lleno() == 1) pthread_cond_wait(&condp2, &el_mutex);

    item.item = 101;
    item.prioridad = 2;
    insert_item(item); /* inserta el marcador de fin de archivo */

    pthread_cond_broadcast(&condc);
    pthread_mutex_unlock(&el_mutex);
    
    fclose(archivo); /* cierra el fichero de texto de entrada        */

    printf("[PROD2] Thread finalizado.\n");
    pthread_exit(NULL); /* termina el thread del productor correctamente */
}

/* ============================================================
 * Función del thread PRODUCTOR3
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
void *hilo_productor3(void *arg){
    /* arg no se usa; el fichero y el buffer son variables globales */
    (void)arg;

    /* Abre el fichero de texto de entrada en modo lectura */
    FILE *archivo = fopen("archivo3.txt", "r");
    if(archivo == NULL){
        printf("[PROD3] Error: no se puede abrir archivo.txt\n");
        pthread_exit(NULL); /* termina el thread si no existe el fichero */
    }

    printf("[PROD3] Thread iniciado. Leyendo archivo.txt...\n");

    /* -----------------------------------------------------------
     * Bucle principal del productor:
     * Lee un entero por iteración hasta EOF o MAX_ITER.
     * ----------------------------------------------------------- */
    elemento_priorizado item;
    item.item = 1; /* entero leído del fichero (int para comparar con EOF)
    		     * se inicializa a 1 para que sea distinto a EOF y podamos pasar al bucle.
		     * Después de cumplir la condición del while, se sobreescribe con produce_item */
    item.prioridad = 3;

    for(int iter = 0; iter < MAX_ITER; iter++){
	/* Salimos del bucle en caso de terminar de leer el archivo */
	if(item.item == 101) break;
        
	/* Acceso exclusivo al buffer */
	pthread_mutex_lock(&el_mutex);

	/* Mientras el buffer está lleno (la función devuelve 1) esperamos */
	while(buffer_lleno() == 1) pthread_cond_wait(&condp3, &el_mutex);

	/* === SECCIÓN CRÍTICA ======================================= */
	/* En este caso, producimos un item con prioridad ínfima */
        item = produce_item(archivo,3); /* Leer el un entero del archivo y se suma a la variable global */

	/* Aquí se pueden producir las carreras críticas    */
	insert_item(item); /* inserta el entero en el buffer compartido   */
   	/* =========================================================== */

	/* Despertamos al consumidor, porque existe un item en el buffer para consumir */
	pthread_cond_signal(&condc);

	/* Liberamos acceso al buffer */
	pthread_mutex_unlock(&el_mutex);

	/* Retardo de velocidad fuera de la región crítica */
        sleep_productor(); 
    }

    /* -------------------------------------------------------
     * Inserta el token 101 de fin de fichero para que el
     * consumidor sepa que no llegarán más datos.
     * Debemos repetir el procedimiento de inserción con las
     * variables de condición y el mutex.
     * ------------------------------------------------------- */
    pthread_mutex_lock(&el_mutex);

    while(buffer_lleno() == 1) pthread_cond_wait(&condp3, &el_mutex);

    item.item = 101;
    item.prioridad = 3;
    insert_item(item); /* inserta el marcador de fin de archivo */

    pthread_cond_broadcast(&condc);
    pthread_mutex_unlock(&el_mutex);
    
    fclose(archivo); /* cierra el fichero de texto de entrada        */

    printf("[PROD3] Thread finalizado.\n");
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
    int fin_hilo1 = 0, fin_hilo2 = 0, fin_hilo3 = 0;	/* variables para salir del bucle cuando terminen los 3 productores */

    printf("[CONS] Thread iniciado. Esperando datos...\n");

    /* -----------------------------------------------------------
     * Bucle principal del consumidor:
     * Continúa hasta que se ha detectado EOF y la pila está vacía,
     * o se alcanzan MAX_ITER iteraciones.
     * ----------------------------------------------------------- */
    for(int iter = 0; iter < MAX_ITER; iter++){
	/* Salimos al detectar los 3 EOF y en cuando la pila esté vacía */
	if(fin_hilo1 && fin_hilo2 && fin_hilo3 && (buffer_vacio()==0)) break;

	/* Acceso exclusivo al buffer */
	pthread_mutex_lock(&el_mutex);

	/* Esperamos mientras el buffer está vacío y los productores no han terminado de leer los archivos */
	while(buffer_vacio() == 0 && !fin_hilo1 && !fin_hilo2 && !fin_hilo3) pthread_cond_wait(&condc, &el_mutex);

        /* === SECCIÓN CRÍTICA ======================================= */
	/* Se pueden producir carreras críticas al buscar items en las posiciones */
        elemento_priorizado item = remove_item(); /* retira del buffer */
        /* === FIN SECCIÓN CRÍTICA =================================== */

	/* Salimos si leemos el final de todos los archivos del buffer */
	if(item.item == 101){
		if(item.prioridad == 1) fin_hilo1 = 1;
		if(item.prioridad == 2) fin_hilo2 = 1;
		if(item.prioridad == 3) fin_hilo3 = 1;
	}

        /* Sumamos el último item retirado del buffer */
        consume_item(item); /* consumimos el item */

   	/* Despertamos al productor en función de la prioridad */
	if(item.prioridad == 1) pthread_cond_signal(&condp1);
	if(item.prioridad == 2) pthread_cond_signal(&condp2);
	if(item.prioridad == 3) pthread_cond_signal(&condp3);

	/* Liberamos el acceso al buffer */
	pthread_mutex_unlock(&el_mutex);

	/* sleep aleatorio pedido por el enunciado */
	sleep_consumidor();
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
        buffer[i].item = 0; /* marca cada celda del buffer como vacía al inicio */
	buffer[i].prioridad = 0;
    }

    /* -------------------------------------------------------
     * Inicialización de variables a cero
     * ------------------------------------------------------- */
    suma_prod1 = 0;
    suma_prod2 = 0;
    suma_prod3 = 0;
    suma_cons1 = 0;
    suma_cons2 = 0;
    suma_cons3 = 0;
    sig_pos = 0;

    /* -------------------------------------------------------
     * Inicialización del mutex y de las variables de condición
     * ------------------------------------------------------- */
    pthread_mutex_init(&el_mutex, 0);

    pthread_cond_init(&condc, 0); /* Variable de condición del consumidor */
    pthread_cond_init(&condp1, 0); /* Variable de condición del productor */
    pthread_cond_init(&condp2, 0); /* Variable de condición del productor */
    pthread_cond_init(&condp3, 0); /* Variable de condición del productor */

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
    pthread_t tid_prod1, tid_prod2, tid_prod3; /* identificador del thread productor  */
    pthread_t tid_cons; /* identificador del thread consumidor */

    /* Crea el thread productor; ejecutará hilo_productor() */
    if(pthread_create(&tid_prod1, NULL, hilo_productor1, NULL) != 0){
        perror("Error al crear thread productor1"); /* mensaje de error del sistema */
        exit(1);
    }
    if(pthread_create(&tid_prod2, NULL, hilo_productor2, NULL) != 0){
        perror("Error al crear thread productor2"); /* mensaje de error del sistema */
        exit(1);
    }
    if(pthread_create(&tid_prod3, NULL, hilo_productor3, NULL) != 0){
        perror("Error al crear thread productor3"); /* mensaje de error del sistema */
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
    pthread_join(tid_prod1, NULL); /* espera a que termine el thread productor  */
    pthread_join(tid_prod2, NULL); /* espera a que termine el thread productor  */
    pthread_join(tid_prod3, NULL); /* espera a que termine el thread productor  */
    pthread_join(tid_cons, NULL); /* espera a que termine el thread consumidor */

    printf("[MAIN] Todos los threads han terminado.\n\n");

    /* -------------------------------------------------------
     * Imprime los resultados de todos los threads
     * ------------------------------------------------------- */
    printf("Suma [PROD1] = %d\n", suma_prod1);
    printf("Suma [CONS] de [PROD1] = %d\n", suma_cons1);
    printf("\nSuma [PROD2] = %d\n", suma_prod2);
    printf("Suma [CONS] de [PROD2] = %d\n", suma_cons2);
    printf("\nSuma [PROD3] = %d\n", suma_prod3);
    printf("Suma [CONS] de [PROD3] = %d\n", suma_cons3);

    /* -------------------------------------------------------
     * Destruimos el mutex y las variables de condición
     * ------------------------------------------------------- */
    pthread_cond_destroy(&condc);
    pthread_cond_destroy(&condp1);
    pthread_cond_destroy(&condp2);
    pthread_cond_destroy(&condp3);

    pthread_mutex_destroy(&el_mutex);

    return 0;
}

//EJECUCION Y COMPILACION:

// gcc -o prod_cons_prioridad prod_cons_prioridad.c -pthread
// ./prod_cons_prioridad
