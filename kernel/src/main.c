#include "main.h"

t_log *logger;
t_config *config;
// sockets servidor
int socket_servidor;
int socket_cliente_io;
// clientes con el CPU
int cliente_cpu_dispatch;
int cliente_cpu_interrupt;
int cliente_memoria;
// hilo servidor I/O
pthread_t hilo_servidor_io;
// otro
int contadorProcesos = 0;
t_queue *cola_procesos;
int quantum;

main(int argc, char *argv[])
{
    logger = iniciar_logger("kernel.log", "KERNEL", argc, argv);
    config = iniciar_config("kernel.config");
    cola_procesos = queue_create();
    // Instancio el quantum desde el config
    quantum = config_get_int_value(config, "QUANTUM");

    // harcodeo de la cola de procesos
    t_PCB *pcb1 = crear_pcb(&contadorProcesos, quantum);
    t_PCB *pcb2 = crear_pcb(&contadorProcesos, quantum);
    t_PCB *pcb3 = crear_pcb(&contadorProcesos, quantum);
    t_PCB *pcb4 = crear_pcb(&contadorProcesos, quantum);
    t_PCB *pcb5 = crear_pcb(&contadorProcesos, quantum);

    queue_push(cola_procesos, pcb1);
    queue_push(cola_procesos, pcb2);
    queue_push(cola_procesos, pcb3);
    queue_push(cola_procesos, pcb4);
    queue_push(cola_procesos, pcb5);
    

    // PARTE CLIENTE
    /*
        if (generar_clientes()) // error al crear los clientes de cpu
            return EXIT_FAILURE;

        t_PCB *pcb = crear_pcb(&contadorProcesos, 3);
        t_paquete *p = crear_paquete();
        pcb->AX = (uint8_t)15;
        pcb->DI = (uint32_t)1231156;
        empaquetar_pcb(p, pcb);
        enviar_paquete(p, cliente_cpu_dispatch, logger);

    // CREACION HILO SERVIDOR I/O

    pthread_create(&hilo_servidor_io, NULL, atender_servidor_io, NULL);
    pthread_join(hilo_servidor_io, NULL);
    */

    // PARTE CONSOLA INTERACTIVA
    int seguir = 1;
    while (seguir)
    {
        // mostrar_menu();
        char *comandoLeido;
        comandoLeido = readline(">"); // Lee de consola lo ingresado
        char **comandoSpliteado = string_split(comandoLeido, " ");

        if (string_equals_ignore_case("INICIAR_PROCESO", comandoSpliteado[0]) && comandoSpliteado[1] != NULL)
        {
            log_info(logger, "Entraste a iniciar proceso para el proceso: %s", comandoSpliteado[1]);
        }
        else if (string_equals_ignore_case("PROCESO_ESTADO", comandoSpliteado[0]) && comandoSpliteado[1] != NULL)
        {
            log_info(logger, "Entraste a Procesos de Estado y son los siguientes: %s"); //lista de procesos
        }
        else if (string_equals_ignore_case("FINALIZAR_PROCESO", comandoSpliteado[0]) && comandoSpliteado[1] != NULL)
        {
            log_info(logger, "Finalizando Estado: %s");
            seguir = 0;
        }
        else if (string_equals_ignore_case("INICIAR_PLAFICACION", comandoSpliteado[0])&& comandoSpliteado[1] != NULL)
        {
        }
        else if (string_equals_ignore_case("DETENER_PLANIFICACION", comandoSpliteado[0]) && comandoSpliteado[1] != NULL)
            seguir = 0;
        else if (string_equals_ignore_case("EJECUTAR_SCRIPT", comandoSpliteado[0]))
        {
            log_info(logger, "Entraste a Ejecutar Script, ingresa el path: %s", comandoSpliteado[1]);
        }
        else if (string_equals_ignore_case("MULTIPROGRAMACION", comandoSpliteado[0]))
        {
            log_info(logger, "Entraste a Multiprogramacion, ingrese valor: %d", comandoSpliteado[1]);
        }
        else if (string_equals_ignore_case("SALIR", comandoSpliteado[0]) && comandoSpliteado[1] != NULL)
        {
            seguir = 0;
        }
        else {log_info(logger, "COMANDO NO VALIDO");}// hacer logger comando no valido
        
    }

    liberar_conexion(cliente_cpu_dispatch, logger);
    liberar_conexion(cliente_cpu_interrupt, logger);
    liberar_conexion(cliente_memoria, logger);
    destruir_logger(logger);
    destruir_config(config);

    return EXIT_SUCCESS;
}

int generar_clientes()
{
    cliente_cpu_dispatch = crear_conexion(config, "IP_CPU", "PUERTO_CPU_DISPATCH", logger);
    cliente_cpu_interrupt = crear_conexion(config, "IP_CPU", "PUERTO_CPU_INTERRUPT", logger);
    cliente_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);

    if (cliente_cpu_dispatch == -1 || cliente_cpu_interrupt == -1)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

void *atender_servidor_io(void *arg)
{
    // PARTE SERVIDOR
    socket_servidor = iniciar_servidor(config, "PUERTO_ESCUCHA");

    log_info(logger, "Servidor %d creado.", socket_servidor);

    socket_cliente_io = esperar_cliente(socket_servidor, logger); // hilarlo

    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(socket_cliente_io, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(socket_cliente_io, logger);
            log_info(logger, "Recibi el mensaje: %s.", mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(socket_cliente_io, logger);
            log_info(logger, "Recibi un paquete.");
            // hago algo con el paquete
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", socket_cliente_io);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Recibi una operacion rara (%d), termino el servidor.", operacion);
            destruir_logger(logger);
            destruir_config(config);
            return EXIT_FAILURE;
        }
    }
}