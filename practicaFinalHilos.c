/*
* @authors: Alberto Santamaria Peñalba
* Programa concurrente encargado de descodificar una imagen de caracteres dado un
* fichero codificado
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>

#define NARG 5

/**
 * Estructura de datos pasada por parametro al hilo lector
 * @property {int} nfil - número de filas totales
 * @property {int} ncol - número de columnas totales
 * @property {int} tbuffer - el tamaño del buffer en el que se almacenarán los datos correctos del fichero codificado
 * @property {int} nhilos - número total de hilos descodificadores
 * @property {char} nombreFichCodificado - El nombre del fichero codificado
 */
typedef struct dato_tlector
{
    int nfil;
    int ncol;
    int tbuffer;
    int nhilos;
    char *nombreFichCodificado;
}dato_tlector;

/**
 * Estructura de datos pasada por parametro al hilo descodificador
 * @property {int} id - el id del decodificador.
 * @property {int} col_ini - el numero de la columna más bajo que descodificará el hilo descodificador
 * @property {int} col_fin - el numero de la columna más alto que descodificará el hilo descodificador
 * @property {int} nhilos - número total de hilos descodificadores
 * @property {int} tbuffer - el tamaño del buffer en el que se almacenarán los datos correctos del fichero codificado
 */
typedef struct dato_tdescodificador
{
    int id;
    int col_ini;
    int col_fin;
    int nhilos;
    int tbuffer;
}dato_tdescodificador;

/**
 * Estructura de datos pasada por parametro al hilo dibujante
 * @property {int} nfil - número de filas totales
 * @property {int} ncol - número de columnas totales
 * @property {int} nhilos - número total de hilos descodificadores
 * @property {char} nombreFichDescoodificado - El nombre del archivo donde se almacenará la imagen descodificada.
 */
typedef struct dato_tdibujante
{
    int nfil;
    int ncol;
    int nhilos;
    char *nombreFichDescoodificado;
}dato_tdibujante;

/**
 * Estructura de los datos que introducimos al buffer circular
 * @property {int} nfils - número de la fila donde se encuentra el caracter
 * @property {int} ncols - número de la columna donde se encuentra el caracter
 * @property {int} ascii - valor entero correspondiente al ascii
 */
typedef struct dato_t
{
    int nfils;
    int ncols;
    int ascii;
}dato_t;

/**
 * Estructura de los datos que introducimos a la listra enlazada
 * @property {int} nfils - número de la fila donde se encuentra el caracter
 * @property {int} ncols - número de la columna donde se encuentra el caracter
 * @property {char} ascii - caracter que escribiremos en el fichero descodificado en su fil y col 
 */
typedef struct datolista_t
{
    int nfils;
    int ncols;
    char ascii;
}datolista_t;

/**
 * Estructura de la lista enlazada
 * @property datos - estructura de datos datolista_t
 * @property sig - puntero al siguiente elemento de la lista
 */
typedef struct lista{
    struct datolista_t datos;
    struct lista *sig;
}lista;

/**
 * Funcion que crea un nuevo nodo.
 * 
 * @return un puntero a una estructura lista.
 */
lista *creanodo(void){
    lista *nl;
    if((nl = (lista *) malloc(sizeof(lista))) == NULL)
    {
        printf("Error asignando memoria1\n");
        exit(-1);
    }
    return nl;
}

/**
 * Funcion encargada de insertar un nodo al final de la lista
 * 
 * @param l puntero al primer nodo de la lista
 * @param x los datos a insertar al final de la lista
 * 
 * @return la nueva lista
 */
lista *insertafinal(lista *l, datolista_t x){
    struct lista *p, *q;
    q = creanodo();
    q->datos = x;
    q->sig = NULL;
    if (l == NULL)
        return q;
    p = l;
    while (p->sig != NULL)
        p = p->sig;
    p->sig = q;
    return l;
}

dato_t *buffer_circular;
int *contador_descodificadores;
lista *l = NULL;
bool lector_trabaje = true;
int desco_trabajen=0, sig_llenar=0, sig_vaciar = 0, lineasTotales=0, lineaCorrecta=0, lineaIncorrecta=0, caracteresTotales=0, caracterCorrecto=0, caracterIncorrecto=0;
FILE *fdatos;

sem_t hay_espacio;
sem_t *hay_dato;
sem_t mutex_fdatos;
sem_t mutex_buffer;
sem_t mutex_sig_llenar;
sem_t puedo_dibujar;

/**
 * Comprueba si una cadena es un número válido
 * @param cadena La cadena que se va a validar.
 * @return 1 si la cadena no es un numero mayor que 0, o 0 si es un numero mayor que 0
 */
int validar(char *cadena){
    for(int i=0; i<strlen(cadena); i++){
        if(!(isdigit(cadena[i]))||cadena[0]=='0'){
            return 1;
        }
    }
    return 0;
}

/**
 * Hilo encargado de leer el archivo codificado y poner los datos correctos en el buffer
 * @param arg estructura de datos dato_tlector
 */
void *lector(void *arg){
    dato_tlector *dato;
    dato = (dato_tlector *) arg;
    dato_t *dat;
    int i, fila, columna, ascii;
    char k;
    FILE *fcodificado = fopen(dato->nombreFichCodificado , "r");
    if((contador_descodificadores = (int*) malloc(sizeof(int)*dato->tbuffer)) == NULL)
    {
        printf("Error asignando memoria2\n");
        exit(-1);
    }
    if ((buffer_circular = (dato_t*) malloc(sizeof(dato_t)*dato->tbuffer)) == NULL)
    {
        printf("Error asignando memoria2\n");
        exit(-1);
    }
    fscanf(fcodificado, "%c,%d,%d", &k, &fila, &columna);
    while(fscanf(fcodificado, "%d,%d,%d", &fila, &columna, &ascii) != EOF){
        lineasTotales=lineasTotales+1;
        if((dato->nfil < fila)||(fila < 0))
            lineaIncorrecta++;
        else if((dato->ncol < columna)||(columna < 0))
            lineaIncorrecta++;
        else
        {
            //SECCION CRITICA (buffer circular)
            dat = (dato_t*) malloc(sizeof(dato_t));
            dat->nfils = fila;
            dat->ncols = columna;
            dat->ascii = ascii;
            lineaCorrecta++;
            sem_wait(&hay_espacio);
            sem_wait(&mutex_sig_llenar);
            i = sig_llenar;
            sig_llenar = (sig_llenar+1)%dato->tbuffer;
            sem_post(&mutex_sig_llenar);
            buffer_circular[i] = *dat;
            contador_descodificadores[i]=0;
            for (int k=0;k<dato->nhilos;k++)
                sem_post(&hay_dato[k]);
            free(dat);
            }   
    }
    //SECCION CRITICA (fichero de datos)
    sem_wait(&mutex_fdatos);
    fprintf(fdatos, "Lineas Totales: '%d'   Lineas Correctas: '%d'  Lineas Incorrectas: '%d'\n", lineasTotales, lineaCorrecta, lineaIncorrecta);
    sem_post(&mutex_fdatos);
    fclose(fcodificado);
    //Introducimos caracter fin de buffer
    sem_wait(&hay_espacio);
    buffer_circular[(i+1)%dato->tbuffer].nfils = -1;
    for (int x=0;x<dato->nhilos;x++)
        sem_post(&hay_dato[x]);
}

/**
 * Hilo encargado de leer el buffer circular, y si los datos están en el rango de columnas que se le han asignado, los introduce
 * en la lista enlazada. Puede haber tantos hilos descodificadores como quiera el usuario (mínimo 1)
 * 
 * @param arg estructura de datos dato_tdescodificador
 */
void *descodificador(void *arg){
    dato_tdescodificador *dato = (dato_tdescodificador *) arg;
    int i=0;
    char j;
    datolista_t k;
    dato_t e;
    while(lector_trabaje){
        //SECCION CRITICA (buffer circular)
        sem_wait(&hay_dato[dato->id]);
        e = buffer_circular[i];
        sem_wait(&mutex_buffer);
        if(e.nfils<0){
            lector_trabaje = false;
        }
        else{
            contador_descodificadores[i]++;
            if((dato->col_ini <= buffer_circular[i].ncols) && (buffer_circular[i].ncols <= dato->col_fin)){
                //comprobar si esta en el rango de columnas
                if((32 <= (buffer_circular[i].ascii)) && ((buffer_circular[i].ascii) <= 126)){
                    //descodificar dato y guardar en lista enlazada
                    caracterCorrecto++;
                    caracteresTotales++;
                    k.nfils=e.nfils;
                    k.ncols=e.ncols;
                    k.ascii=e.ascii;
                    l = insertafinal(l, k);
                    sem_post(&puedo_dibujar);
                }
                else{
                    //descartar dato
                    caracterIncorrecto++;
                    caracteresTotales++;                
                }
            }
            //esperar a que todos los descodificadores lean cada posicion del buffer
            if(contador_descodificadores[i] == dato->nhilos)
                sem_post(&hay_espacio);             
            i = (i+1)%dato->tbuffer;
        }
        sem_post(&mutex_buffer);
    }
    desco_trabajen++;
    //SECCION CRITICA (fichero de datos)
    sem_wait(&mutex_fdatos);
    fprintf(fdatos, "HILO:'%d', Columnas Asignadas:'%d'-'%d'\n", dato->id, dato->col_ini, dato->col_fin);
    sem_post(&mutex_fdatos);
}

/**
 * Lee la lista enlazada según va recibiendo datos y los guarda en una matriz.
 * Posteriormente escribe los datos de la matriz en un nuevo archivo.
 * 
 * @param arg estructura de datos dato_tdibujante
 */
void *dibujante(void *arg){
    char **matriz;
    dato_tdibujante *dato = (dato_tdibujante *) arg;
    if((matriz = (char **)malloc(sizeof(char *)*dato->nfil)) == NULL)
    {
        printf("Error asignando memoria3\n");
        exit(-1);
    }
    for (int i = 0; i<dato->nfil; i++)
        if((matriz[i] = (char *)malloc(sizeof(char)*dato->ncol)) == NULL)
        {
            printf("Error asignando memoria4\n");
            exit(-1);
        }
    while(desco_trabajen<dato->nhilos){
        sem_wait(&puedo_dibujar);
        while (l != NULL){
            matriz[l->datos.nfils][l->datos.ncols] = l->datos.ascii;
            l = l->sig;
        }
    }
   //liberamos lista enlazada
    while (l != NULL){
        free(l);
        l = l->sig;
    }
    FILE *fdescodificado = fopen(dato->nombreFichDescoodificado,"wt");
    for(int i = 0; i < dato->nfil; i++){
        for (int j = 0; j < dato->ncol; j++){
            fprintf(fdescodificado, "%c", matriz[i][j]);
        }
        fprintf(fdescodificado, "\n");
    }
    //liberamos matriz
    for(int i = 0; i < dato->nfil; i++)
        free(matriz[i]);
    free(matriz);
    fclose(fdescodificado);
}

int main(int argc, char **argv){

    dato_tlector *param_lector;
    dato_tdescodificador *param_descodificador;
    dato_tdibujante *param_dibujante;
    char caracter;
    int nfils, ncols, cols_repartidas, cols_restantes, contador_cols, nhilos;
    FILE *fcodificado;
    //Control de errores en los datos que introduce el usuario
    if(argc!=NARG+1){
        printf("Numero de parametros incorrecto\n");
        exit(1);
    }
    if((fcodificado=fopen(argv[1],"r"))==NULL){
        printf("El fichero codificado no existe\n");
        exit(2);
    }
    fscanf(fcodificado, "%c,%d,%d", &caracter, &nfils, &ncols);
    fclose(fcodificado);

    if((caracter != 's') || (nfils<0) || (ncols<0)){
        printf("Error en la primera linea del fichero fcodificado\n");
        exit(2);
    }

    if(validar(argv[3])==1){
        printf("El tercer parametro es incorrecto. Tiene que ser un numero mayor que 0 sin signos\n");
        exit(3);
    }
    if(validar(argv[4])==1){
        printf("El cuarto parametro es incorrecto. Tiene que ser un numero mayor que 0 sin signos\n");
        exit(3);
    }
    if(atoi(argv[4]) > ncols)
        nhilos = ncols;
    else
        nhilos = atoi(argv[4]);
    hay_dato = (sem_t*)malloc(sizeof(sem_t)*nhilos);
    for(int i = 0; i < nhilos; i++)
        sem_init(&hay_dato[i], 0, 0);
    sem_init(&hay_espacio, 0, atoi(argv[3]));
    sem_init(&mutex_buffer, 0, 1);
    sem_init(&mutex_fdatos, 0, 1);
    sem_init(&mutex_sig_llenar, 0, 1);
    sem_init(&puedo_dibujar, 0, 0);

    pthread_t hlector;
    pthread_t hdescodificador[nhilos];
    pthread_t hdibujante;

    param_lector = (dato_tlector*)malloc(sizeof(dato_tlector));
    param_lector->nombreFichCodificado = argv[1];
    param_lector->tbuffer = atoi(argv[3]);
    param_lector->nfil = nfils;
    param_lector->ncol = ncols;
    param_lector->nhilos = nhilos;

    fdatos = fopen("fdatos", "wt");
    pthread_create (&hlector , NULL , lector , (void *)param_lector);

    cols_repartidas = ncols / nhilos;
    cols_restantes = ncols % nhilos;
    param_descodificador = (dato_tdescodificador*)malloc(sizeof(dato_tdescodificador)*nhilos);
    contador_cols = 0;
    for(int i = 0; i < nhilos; i++){
        param_descodificador[i].id = i;
        param_descodificador[i].col_ini = contador_cols;
        contador_cols = contador_cols + cols_repartidas;
        if (cols_restantes != 0)
        {
            contador_cols++;
            param_descodificador[i].col_fin = contador_cols-1;
            cols_restantes--;
        }
        else
            param_descodificador[i].col_fin = contador_cols-1;
            param_descodificador[i].tbuffer = atoi(argv[3]);
            param_descodificador[i].nhilos = nhilos;
        pthread_create (&hdescodificador[i] , NULL , *descodificador , (void *)&param_descodificador[i]);
    }
    
    param_dibujante = (dato_tdibujante*)malloc(sizeof(dato_tdibujante));
    param_dibujante->nfil = nfils;
    param_dibujante->ncol = ncols;
    param_dibujante->nhilos = nhilos;
    param_dibujante->nombreFichDescoodificado = argv[2];

    pthread_create (&hdibujante , NULL , *dibujante , (void *)param_dibujante );
    
    pthread_join(hlector, NULL);
    free(param_lector);
    for (int i = 0; i < nhilos; i++){
        pthread_join(hdescodificador[i], NULL);
    }
    sem_wait(&mutex_fdatos);
    fprintf(fdatos, "Caracteres consumidos por los descodificadores:'%d', Caracteres válidos: '%d', Caracteres inválidos: '%d'\n", caracteresTotales, caracterCorrecto, caracterIncorrecto);
    sem_post(&mutex_fdatos);
    free(contador_descodificadores);
    free(buffer_circular);
    free(param_descodificador);
    fclose(fdatos);
    for(int i = 0; i < nhilos; i++)
        sem_destroy(&hay_dato[i]);
    sem_post(&puedo_dibujar);

    pthread_join(hdibujante, NULL);
    free(param_dibujante);
}