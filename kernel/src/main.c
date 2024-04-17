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
int contadorProcesos = 123;
int quantum;
int grado_multiprogramacion;
e_algoritmo_planificacion algoritmo_planificacion;
// lista de pcbs
t_list *lista_de_pcbs;
// Queues de estados
t_queue *cola_NEW;
t_queue *cola_READY;
t_queue *cola_BLOCKED;
t_queue *cola_EXIT; // esta es mas q nada para desp poder ver los procesos q ya terminaron
// Queue de vrr
// esto se hara en su respectivo momento

int main(int argc, char *argv[])
{
    logger = iniciar_logger("kernel.log", "KERNEL", argc, argv);
    config = iniciar_config("kernel.config");

    // Instancio variables de configuracion desde el config
    quantum = config_get_int_value(config, "QUANTUM");
    grado_multiprogramacion = config_get_int_value(config, "GRADO_MULTIPROGRAMACION");
    algoritmo_planificacion = obtener_algoritmo_planificacion(config_get_string_value(config, "ALGORITMO_PLANIFICACION"));
    lista_de_pcbs = list_create();
    instanciar_colas();

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

        if ((string_equals_ignore_case("INICIAR_PROCESO", comandoSpliteado[0]) || string_equals_ignore_case("IP", comandoSpliteado[0])) && comandoSpliteado[1] != NULL)
        {
            log_debug(logger, "Entraste a INICIAR_PROCESO, path: %s.", comandoSpliteado[1]);
            crear_proceso(comandoSpliteado[1]);
        }
        else if ((string_equals_ignore_case("PROCESO_ESTADO", comandoSpliteado[0]) || string_equals_ignore_case("PE", comandoSpliteado[0])) && comandoSpliteado[1] != NULL)
        {
            log_debug(logger, "Entraste a PROCESO_ESTADO."); // lista de procesos
            log_info(logger, "Listado de procesos por estado:");
        }
        else if ((string_equals_ignore_case("FINALIZAR_PROCESO", comandoSpliteado[0]) || string_equals_ignore_case("FP", comandoSpliteado[0])) && comandoSpliteado[1] != NULL)
        {
            log_debug(logger, "Entraste a FINALIZAR_PROCESO para el proceso: %s.", comandoSpliteado[1]);
        }
        else if (string_equals_ignore_case("INICIAR_PLAFICACION", comandoSpliteado[0]) || string_equals_ignore_case("IPL", comandoSpliteado[0]))
        {
            log_debug(logger, "Entraste a INICIAR_PLAFICACION.");
        }
        else if (string_equals_ignore_case("DETENER_PLANIFICACION", comandoSpliteado[0]) || string_equals_ignore_case("DP", comandoSpliteado[0]))
        {
            log_debug(logger, "Entraste a DETENER_PLANIFICACION.");
        }
        else if (string_equals_ignore_case("EJECUTAR_SCRIPT", comandoSpliteado[0]) || string_equals_ignore_case("ES", comandoSpliteado[0]))
        {
            log_debug(logger, "Entraste a EJECUTAR_SCRIPT, path: %s.", comandoSpliteado[1]);
        }
        else if ((string_equals_ignore_case("MULTIPROGRAMACION", comandoSpliteado[0]) || string_equals_ignore_case("MP", comandoSpliteado[0])) && comandoSpliteado[1] != NULL)
        {
            log_debug(logger, "Entraste a MULTIPROGRAMACION, nuevo grado: %s.", comandoSpliteado[1]);
            log_debug(logger, "Se va a modificar el grado de multiprogramacion de %d a %s.", grado_multiprogramacion, comandoSpliteado[1]);
            grado_multiprogramacion = atoi(comandoSpliteado[1]);
        }
        else if (string_equals_ignore_case("SALIR", comandoSpliteado[0]) || string_equals_ignore_case("S", comandoSpliteado[0]))
        {
            log_debug(logger, "Entraste a SALIR, se va a terminar el modulo", comandoSpliteado[1]);
            seguir = 0;
        }
        else
        {
            log_error(logger, "COMANDO NO VALIDO: %s", comandoSpliteado[0]);
        }
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

// planificacion de largo plazo

void crear_proceso(char *path)
{
    /* COSAS QUE HACE:
    1- le pide a memoria q cree en memoria el proceso pasandole el path (tengo q pasarle el id tmb? desp como los relaciono?)
    2- crea la instancia del pcb para dicho proceso
    3- lo mete a a la lista
    3- mete el id a la cola de new (otro codigo es el q se encarga de moverlo a ready)
    */
    // enviar_mensaje(path, cliente_memoria, logger); // esto solo en si capaz no es suficiente, creo q ademas hay q decirle a memoria q quiero hacer

    t_PCB *nuevo_pcb = crear_pcb(&contadorProcesos, quantum);

    list_add(lista_de_pcbs, nuevo_pcb);

    queue_push(cola_NEW, nuevo_pcb->processID); // ahora las colas solo van a tener los ids (para manejarlo mas simple)

    log_info(logger, "Se crea el proceso %d en NEW.", nuevo_pcb->processID); // log obligatorio
}

void eliminar_proceso()
{
}

e_algoritmo_planificacion obtener_algoritmo_planificacion(char *algo)
{
    if (string_equals_ignore_case("RR", algo))
        return RR;
    else if (string_equals_ignore_case("VRR", algo))
        return VRR;
    return FIFO;
}

void instanciar_colas()
{
    cola_NEW = queue_create();
    cola_READY = queue_create();
    cola_EXIT = queue_create();
    cola_BLOCKED = queue_create();
}

// funciones de manejo de lista_de_pcb
