#include "bloques.h"

t_bloques *crear_bloques(char *path, uint32_t cant_bloques, uint32_t tam_bloque, t_log *logger)
{
    FILE *fbloques = fopen(path, "ab");
    log_debug(logger, "Se creo el archivo de bloques en el path: %s", path);

    uint32_t size = cant_bloques * tam_bloque;
    truncate(path, size);
    log_debug(logger, "Se trunco el archivo para que tenga size: %u", size);

    t_bloques *bq = malloc(sizeof(t_bloques));
    bq->path = string_duplicate(path);
    bq->size_bloque = tam_bloque;
    bq->cant_bloques = cant_bloques;

    int fd_bloques = open(bq->path, O_RDWR);
    bq->bloques = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd_bloques, 0);

    if (bq->bloques == MAP_FAILED)
    {
        log_error(logger, "Fallo el mmap.");
        return 0;
    }

    return bq;
}

void *leer_bloque(t_bloques *bloques, uint32_t pos_bloque, uint32_t offset)
{
}

uint8_t escribir_bloque(t_bloques *bloques, uint32_t pos_bloque, uint32_t offset) { return 0; }