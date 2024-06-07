#include "bitmap.h"

t_bitmap *crear_bitmap(char *path, uint32_t cant_bits, t_log *logger)
{
    FILE *fbitmap = fopen(path, "ab");
    log_debug(logger, "Se creo el archivo de bloques en el path: %s", path);

    // trunco el archivo y lo cierro
    uint32_t cant_en_bytes = cant_bits / 8;
    truncate(path, cant_en_bytes);
    log_debug(logger, "Se trunco el archivo para que tenga size: %u", cant_en_bytes);
    fclose(fbitmap);

    // creo el bitmap a retornar
    t_bitmap *bm = malloc(sizeof(t_bitmap));
    bm->path = string_duplicate(path);
    bm->size_en_bytes = cant_en_bytes;

    return bm;
}

uint8_t esta_bloque_ocupado(t_bitmap *bitmap, uint32_t pos_bloque)
{
    // saco del archivo el bitmap y lo meto en un bitarray
    FILE *fbitmap = fopen(bitmap->path, "r");
    void *dump_bitmap = malloc(bitmap->size_en_bytes);
    fgets(dump_bitmap, bitmap->size_en_bytes, fbitmap);
    t_bitarray *bitarray = bitarray_create(dump_bitmap, bitmap->size_en_bytes);

    // verifico el estado del bloque
    uint8_t estado = bitarray_test_bit(bitarray, pos_bloque);

    // cierro todo y retorno
    fclose(fbitmap);
    bitarray_destroy(bitarray);
    return estado;
}

uint8_t ocupar_bloque(t_bitmap *bitmap, uint32_t pos_bloque)
{
    uint8_t resultado;

    // saco del archivo el bitmap y lo meto en un bitarray
    FILE *fbitmap = fopen(bitmap->path, "w");
    void *dump_bitmap = malloc(bitmap->size_en_bytes);
    fgets(dump_bitmap, bitmap->size_en_bytes, fbitmap);
    t_bitarray *bitarray = bitarray_create(dump_bitmap, bitmap->size_en_bytes);

    // verifico el estado del bloque
    if (bitarray_test_bit(bitarray, pos_bloque))
    { // ya esta ocupado
        resultado = 0;
    }
    // seteo el bit y guardo el bitarray en el archivo
    else
    {
        bitarray_set_bit(bitarray, pos_bloque);
        fprintf(fbitmap, bitarray->bitarray);
        resultado = 1;
    }

    // cierro todo y retorno
    fclose(fbitmap);
    bitarray_destroy(bitarray);
    return resultado;
}

void printear_bitmap()
{
}