#ifndef BLOQUES_H_
#define BLOQUES_H_

#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <commons/memory.h>

typedef struct
{
    char *path;
    uint32_t size_bloque;
    uint32_t cant_bloques;
    char *bloques;
} t_bloques;

t_bloques *crear_bloques(char *path, uint32_t cant_bloques, uint32_t tam_bloque, t_log *logger);
void *leer_dato_bloque(t_bloques *bloques, uint32_t pos_bloque, uint32_t size_dato_leer);
void *leer_bloque(t_bloques *bloques, uint32_t pos_bloque, uint32_t offset);
uint8_t escribir_bloque(t_bloques *bloques, uint32_t pos_bloque, uint32_t offset);

#endif