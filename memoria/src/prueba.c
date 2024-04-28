#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

FILE * archivo_text_proceso;
char *linea = NULL;
size_t longitud = 100;
long *posiciones;
int numeroLinea = 3; 
int contadorLineas = 0;
long posicion;
ssize_t bytesLeidos;


int main(void) {
    archivo_text_proceso = fopen("prueba.txt", "r");
    // leo cada linea y la imprimo por pantalla
    if (archivo_text_proceso == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }


    // Almacena la posición de inicio del archivo
    long inicioArchivo = ftell(archivo_text_proceso);

    printf("La posicion del archivo es %ld\n", inicioArchivo);


    // Encuentra el número total de líneas en el archivo
    while (getline(&linea, &longitud, archivo_text_proceso) != -1) {
        contadorLineas++;
    }

    printf("El archivo tiene %d lineas\n", contadorLineas);

    // Resetea el puntero de archivo al inicio del archivo
    fseek(archivo_text_proceso, inicioArchivo, SEEK_SET);

    // Reserva memoria para almacenar las posiciones de las líneas
    posiciones = (long *)malloc(contadorLineas * sizeof(long));

    // Almacena la posición de inicio de cada línea
    for (int i = 0; i < contadorLineas; i++) {
        posiciones[i] = ftell(archivo_text_proceso);

        if (getline(&linea, &longitud, archivo_text_proceso) == -1) {
            break;
        }
    }


    // Calcula la posición de la línea deseada
    posicion = posiciones[numeroLinea];

    // Mueve el puntero de posición del archivo a la ubicación calculada
    fseek(archivo_text_proceso, posicion, SEEK_SET);

    // Lee la línea deseada
    bytesLeidos = getline(&linea, &longitud, archivo_text_proceso);

    // Imprime la línea
    printf("Contenido de la linea %d: %s", numeroLinea, linea);

    // Libera la memoria y cierra el archivo
    free(posiciones);
    fclose(archivo_text_proceso);

    return 0;
}