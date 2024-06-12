#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <utils/configUtils.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <utils/sockets.h>
#include <commons/bitarray.h>
#include <commons/memory.h>
#include <assert.h>
#include <utils/operacionMemoriaUtils.h>
typedef enum
{
    SOY_CPU = 0,
    SOY_KERNEL,
    SOY_IO
} e_quien_soy;


typedef struct t_memoria_proceso
{
    char **lineas_de_codigo;
    char *nombre_archivo;
    uint32_t *tabla_paginas;
    uint32_t paginas_actuales;
} t_memoria_proceso;

int main(int argc, char *argv[]);
void *servidor_kernel(void *arg);
void *servidor_cpu(void *arg);
void *servidor_entradasalida(void *arg);
int crear_proceso(char *nombre_archivo, uint32_t pid);
t_memoria_proceso *crear_estructura_proceso(char *nombre_archivo, char **lineas_de_codigo);
t_memoria_proceso *encontrar_proceso(uint32_t pid);
void destruir_proceso(uint32_t pid);
void crear_espacio_memoria();
void *esperar_io(void *arg);
uint32_t conseguir_siguiente_marco(uint32_t pid,uint32_t marco);
int hacer_pedido_escritura(t_list* lista);
void* hacer_pedido_lectura(t_list* lista);
char* obtener_instruccion(t_list* lista);
uint32_t obtener_marco(t_list* lista);
#endif