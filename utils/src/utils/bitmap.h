#ifndef BITMAP_H_
#define BITMAP_H_

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
    uint32_t size_en_bytes;
    t_bitarray *bitarray;
} t_bitmap;

t_bitmap *crear_bitmap(char *path, uint32_t cant_bits, t_log *logger);
uint8_t esta_bloque_ocupado(t_bitmap *bitmap, uint32_t pos_bloque);
uint8_t ocupar_bloque(t_bitmap *bitmap, uint32_t pos_bloque);
uint8_t liberar_bloque(t_bitmap *bitmap, uint32_t pos_bloque);

#endif