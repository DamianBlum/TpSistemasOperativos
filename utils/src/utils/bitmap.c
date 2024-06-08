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

    // le meto el bitarray
    int fd_bitmap = open(bm->path, O_RDWR);
    char *dump_bitmap = mmap(NULL, bm->size_en_bytes, PROT_WRITE, MAP_SHARED, fd_bitmap, 0);

    if (dump_bitmap == MAP_FAILED)
    {
        printf("Fallo el mmap.");
        return 0;
    }

    t_bitarray *bitarray = bitarray_create(dump_bitmap, bm->size_en_bytes);
    bm->bitarray = bitarray;

    // fclose(fd_bitmap);

    return bm;
}

uint8_t esta_bloque_ocupado(t_bitmap *bitmap, uint32_t pos_bloque)
{
    // saco del archivo el bitmap y lo meto en un bitarray
    // FILE *fbitmap = fopen(bitmap->path, "r");
    // void *dump_bitmap = malloc(bitmap->size_en_bytes);
    // char *dump_bitmap = calloc(1, bitmap->size_en_bytes + 1);
    // printf("\n ASD: %d \n|%s|\n", string_length(dump_bitmap), dump_bitmap);
    // fread(dump_bitmap, bitmap->size_en_bytes, 1, fbitmap);

    // verifico el estado del bloque
    uint8_t estado = bitarray_test_bit(bitmap->bitarray, pos_bloque);

    // cierro todo y retorno
    // fclose(fbitmap);
    // bitarray_destroy(bitarray);
    // munmap(dump_bitmap, bitmap->size_en_bytes);
    return estado;
}

uint8_t ocupar_bloque(t_bitmap *bitmap, uint32_t pos_bloque)
{
    uint8_t resultado;

    // verifico el estado del bloque
    if (bitarray_test_bit(bitmap->bitarray, pos_bloque))
    { // ya esta ocupado
        resultado = 0;
    }
    // seteo el bit y guardo el bitarray en el archivo
    else
    {
        bitarray_set_bit(bitmap->bitarray, pos_bloque);
        resultado = 1;
        // mem_hexdump(bitmap->bitarray->bitarray, 8);
    }

    // cierro todo y retorno
    return resultado;
}

void printear_bitmap()
{
}