#include "main.h"

t_log *logger;
t_config *config;

// Conexion con kernel
int cliente_kernel;

// Conexion con memoria
int cliente_memoria;

int main(int argc, char *argv[])
{
    logger = iniciar_logger("e-s.log", "ENTRADA-SALIDA", argc, argv);
    config = iniciar_config("e-s.config");

    if (crear_conexiones_de_entrada_salida()) // error al crear las conexiones con kernel y memoria
    {
        return EXIT_FAILURE;
    }

    // hacer lo que toque hacer
    enviar_mensaje("HOLA DESDE ENTRADA/SALIDA A KERNEL", cliente_kernel, logger);
    enviar_mensaje("HOLA DESDE ENTRADA/SALIDA A MEMORIA", cliente_memoria, logger);

    liberar_conexion(cliente_kernel, logger);
    liberar_conexion(cliente_memoria, logger);
    destruir_config(config);
    destruir_logger(logger);

    return EXIT_SUCCESS;
}

int crear_conexiones_de_entrada_salida()
{
    cliente_kernel = crear_conexion(config, "IP_KERNEL", "PUERTO_KERNEL", logger);
    cliente_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);

    if (cliente_kernel == -1 || cliente_memoria == -1)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}