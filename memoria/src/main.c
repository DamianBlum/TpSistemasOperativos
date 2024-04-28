#include "main.h"

t_log *logger;
t_config *config;

// servidor
int socket_servidor;

// clientes
int cliente_kernel;
int cliente_cpu;
int cliente_entradasalida;

// hilos
pthread_t tid[3];

FILE * archivo_text_proceso;


int main(int argc, char *argv[])
{
    logger = iniciar_logger("memoria.log", "MEMORIA", argc, argv);
    config = iniciar_config("memoria.config");

    socket_servidor = iniciar_servidor(config, "PUERTO_ESCUCHA");

    log_info(logger, "Servidor %d creado.", socket_servidor);

    // 3 clientes - CPU - I/O - KERNEL
    // Sabemos que los 3 clientes se conectan en orden: CPU => KERNEL => I/O

    // Recibimos la conexion de la cpu y creamos un hilo para trabajarlo
    cliente_cpu = esperar_cliente(socket_servidor, logger);
    pthread_create(&tid[SOY_CPU], NULL, servidor_cpu, NULL);

    // Recibimos la conexion de la kernel y creamos un hilo para trabajarlo
    cliente_kernel = esperar_cliente(socket_servidor, logger);
    pthread_create(&tid[SOY_KERNEL], NULL, servidor_kernel, NULL);

    // Recibimos la conexion de la I/O y creamos un hilo para trabajarlo
    cliente_entradasalida = esperar_cliente(socket_servidor, logger);
    pthread_create(&tid[SOY_IO], NULL, servidor_entradasalida, NULL);

    pthread_join(tid[SOY_CPU], NULL);
    pthread_join(tid[SOY_KERNEL], NULL);
    pthread_join(tid[SOY_IO], NULL);

    destruir_logger(logger);
    destruir_config(config);

    return EXIT_SUCCESS;
}

void *servidor_kernel(void *arg)
{
    log_info(logger, "El socket cliente %d entro al servidor de Kernel", cliente_kernel);

    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(cliente_kernel, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(cliente_kernel, logger);
            log_info(logger, "Desde cliente %d: Recibi el mensaje: %s.", cliente_cpu, mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(cliente_kernel, logger);
            log_info(logger, "Desde cliente %d: Recibi un paquete.", cliente_cpu);
            // hago algo con el mensaje
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", cliente_cpu, cliente_kernel);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente %d: Recibi una operacion rara (%d), termino el servidor.", cliente_cpu, operacion);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

void *servidor_cpu(void *arg)
{
    log_info(logger, "El socket cliente %d entro al servidor de CPU", cliente_cpu);
    // abro el archivo txt "prueba.txt"para leerlo como pruebo de la comunicacion
    archivo_text_proceso = fopen("prueba.txt", "r");
    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(cliente_cpu, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(cliente_cpu, logger);
            log_info(logger, "Desde cliente %d:Recibi el mensaje: %s.", cliente_cpu, mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(cliente_cpu, logger);
            log_info(logger, "Desde cliente: %d Recibi un paquete.", cliente_cpu);
            char* program_counter = list_get(lista, 0);
            // paso el program counter a int
            int pc = atoi(program_counter);
            // leo el archivo de texto en la linea pc con fgets
            
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", cliente_cpu);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente: %d Recibi una operacion rara (%d), termino el servidor.", operacion);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

void *servidor_entradasalida(void *arg)
{
    log_info(logger, "El socket cliente %d entro al servidor de I/O", cliente_entradasalida);

    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(cliente_entradasalida, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(cliente_entradasalida, logger);
            log_info(logger, "Desde cliente %d: Recibi el mensaje: %s.", cliente_entradasalida, mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(cliente_entradasalida, logger);
            log_info(logger, "Desde cliente %d: Recibi un paquete.", cliente_entradasalida);
            // hago algo con el mensaje
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", cliente_entradasalida, cliente_entradasalida);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente %d: Recibi una operacion rara (%d), termino el servidor.", cliente_entradasalida, operacion);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}