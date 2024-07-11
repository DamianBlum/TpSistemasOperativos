#include "logUtils.h"

t_log *iniciar_logger(char *nombreLog, char *rutaLog, int argc, char *argv[])
{
    t_log *nuevo_logger;
    char *modo;

    // con el argumento defino el log level
    if (argc > 1)
    {
        if (string_equals_ignore_case("-info", argv[1]))
        {
            modo = string_duplicate("INFO");
            nuevo_logger = log_create(nombreLog, rutaLog, 1, LOG_LEVEL_INFO);
        }
        else if (string_equals_ignore_case("-debug", argv[1]))
        {
            modo = string_duplicate("DEBUG");
            nuevo_logger = log_create(nombreLog, rutaLog, 1, LOG_LEVEL_DEBUG);
        }
        else if (string_equals_ignore_case("-error", argv[1]))
        {
            modo = string_duplicate("ERROR");
            nuevo_logger = log_create(nombreLog, rutaLog, 1, LOG_LEVEL_ERROR);
        }
        else if (string_equals_ignore_case("-trace", argv[1]))
        {
            modo = string_duplicate("TRACE");
            nuevo_logger = log_create(nombreLog, rutaLog, 1, LOG_LEVEL_TRACE);
        }
        else if (string_equals_ignore_case("-warning", argv[1]))
        {
            modo = string_duplicate("WARNING");
            nuevo_logger = log_create(nombreLog, rutaLog, 1, LOG_LEVEL_WARNING);
        }
        else if (string_equals_ignore_case("-mute", argv[1]))
        {
            modo = string_duplicate("TRACE - MUTE");
            nuevo_logger = log_create(nombreLog, rutaLog, 0, LOG_LEVEL_TRACE);
        }
        else // si ingreso cualquier otra cosa lo pongo en info
        {
            modo = string_duplicate("INFO");
            nuevo_logger = log_create(nombreLog, rutaLog, 1, LOG_LEVEL_INFO);
        }
    }
    else // si no mando ningun modo lo pongo en info
    {
        modo = string_duplicate("INFO");
        nuevo_logger = log_create(nombreLog, rutaLog, 1, LOG_LEVEL_INFO);
    }

    if (nuevo_logger == NULL)
    {
        printf("Error al crear el logger, termino el programa.\n");
        exit(1);
    }

    log_info(nuevo_logger, "---------- INICIO DEL LOG EN MODO %s ----------\n", modo);
    free(modo);
    return nuevo_logger;
}

void destruir_logger(t_log *logger)
{
    if (logger != NULL)
    {
        log_info(logger, "---------- FIN DEL LOG ----------\n");
        log_destroy(logger);
    }
}