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
int quantum;
int grado_multiprogramacion;        // este es el definido por parametro de config
int grado_multiprogramacion_actual; // y este es un contador de procesos en el sistema, se modifica en el planificador de largo plazo
e_algoritmo_planificacion algoritmo_planificacion;
// lista de pcbs
t_list *lista_de_pcbs;
// Queues de estados
t_queue *cola_NEW;
t_queue *cola_READY;
t_queue *cola_RUNNING; // si, es una cola para un unico proceso, algun problema?
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
        mostrar_menu();
        char *comandoLeido;
        comandoLeido = readline(">"); // Lee de consola lo ingresado
        char **comandoSpliteado = string_split(comandoLeido, " ");

        if ((string_equals_ignore_case("INICIAR_PROCESO", comandoSpliteado[0]) || string_equals_ignore_case("IP", comandoSpliteado[0])) && comandoSpliteado[1] != NULL)
        {
            log_debug(logger, "Entraste a INICIAR_PROCESO, path: %s.", comandoSpliteado[1]);
            crear_proceso(comandoSpliteado[1]);
            evaluar_NEW_a_READY();
            evaluar_READY_a_EXEC(); // esto capaz no deberia estar aca, pero lo pongo xq nose donde mas tendria q atender esto, mas q nada la primera vez q ingreso un proceso, para el resto con el RUNNING -> ALGO alcanza
        }
        else if (string_equals_ignore_case("PROCESO_ESTADO", comandoSpliteado[0]) || string_equals_ignore_case("PE", comandoSpliteado[0]))
        {
            log_debug(logger, "Entraste a PROCESO_ESTADO."); // lista de procesos
            log_info(logger, "Listado de procesos con estado:");

            for (int i = 0; i < list_size(lista_de_pcbs); i++)
            {
                t_PCB *p = list_get(lista_de_pcbs, i);
                log_info(logger, "ID: %d Estado: %s", p->processID, estado_proceso_texto(p->estado));
                log_debug(logger, "ID: %d PC: %d Q: %d Estado: %s AX: %d BX: %d CX: %d DX: %d EAX: %d EBX: %d ECX: %d EDX SI: %d DI: %d", p->processID, p->programCounter, p->quantum, estado_proceso_texto(p->estado), p->AX, p->BX, p->CX, p->DX, p->EAX, p->EBX, p->ECX, p->EDX, p->SI, p->DI);
            }
            log_info(logger, "Fin de la lista.");
        }
        else if ((string_equals_ignore_case("FINALIZAR_PROCESO", comandoSpliteado[0]) || string_equals_ignore_case("FP", comandoSpliteado[0])) && comandoSpliteado[1] != NULL)
        {
            log_debug(logger, "Entraste a FINALIZAR_PROCESO para el proceso: %s.", comandoSpliteado[1]);
            eliminar_proceso();
            evaluar_NEW_a_READY();
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
    grado_multiprogramacion_actual--;

    log_debug(logger, "Grado de multiprogramacion actual: %d", grado_multiprogramacion_actual);
}

// planificacion de corto plazo

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
    cola_RUNNING = queue_create();
    cola_EXIT = queue_create();
    cola_BLOCKED = queue_create();
}

// funciones de manejo de lista_de_pcb

t_PCB *obtener_pcb_de_lista_por_id(int id)
{
    for (int i = 0; i < list_size(lista_de_pcbs); i++)
    {
        t_PCB *pcbAux = list_get(lista_de_pcbs, i);

        if (pcbAux->processID == id)
            return pcbAux;
    }
    return NULL;
}

// cambios de estados

void evaluar_NEW_a_READY()
{ // evalua si puede uno, o varios, procesos en new parsar a ready si da el grado de multiprogramacion
    log_trace(logger, "Voy a evaluar si puedo mover uno o mas procesos de la cola NEW a READY.");

    while (!queue_is_empty(cola_NEW))
    {
        if (grado_multiprogramacion_actual < grado_multiprogramacion)
        {
            uint32_t id = queue_pop(cola_NEW);

            queue_push(cola_READY, id);
            log_trace(logger, "Fue posible mover el proceso %d de NEW a READY.", id);

            grado_multiprogramacion_actual++;
            log_debug(logger, "Grado de multiprogramacion actual: %d", grado_multiprogramacion_actual);
        }
        else
        {
            log_trace(logger, "No puedo porque el grado de multiprogramacion (%d) no admite mas programas en el sistema (actual: %d).", grado_multiprogramacion, grado_multiprogramacion_actual);
            break;
        }
    }
}

void evaluar_READY_a_EXEC()
{
    log_trace(logger, "Entre a READY a EXEC para evaluar si se puede asignar un proceso al CPU.");
    if (queue_is_empty(cola_RUNNING) && !queue_is_empty(cola_READY)) // creo q solo tengo q validar q no este nadie corriendo
    {                                                                // tengo q hacer algo distinto segun cada algoritmo de planificacion
        uint32_t id;

        switch (algoritmo_planificacion)
        {
        case FIFO: // saco el primero
            log_trace(logger, "Entre a planificacion por FIFO.");
            id = queue_pop(cola_READY);
            break;
        case RR: // esto deberia ser lo mismo q fifo
            log_error(logger, "Todavia no desarrollado.");
            id = __UINT32_MAX__; // esto es algo para poder identificar q no esta desarrollado en CPU
            break;
        case VRR:
            log_error(logger, "Todavia no desarrollado.");
            id = __UINT32_MAX__;
            break;
        default:
            log_error(logger, "Esto nunca va a pasar.");
            break;
        }
        log_trace(logger, "El proceso %d va a ser asignado al CPU.", id);
        queue_push(cola_RUNNING, id);
        // ya q estamos le mando a cpu el contexto de ejecucion del proceso elegido
        t_PCB *pcb_elegido = obtener_pcb_de_lista_por_id(id);
        t_paquete *paquete_con_pcb = crear_paquete();
        empaquetar_pcb(paquete_con_pcb, pcb_elegido);
        enviar_paquete(paquete_con_pcb, cliente_cpu_dispatch, logger);

        // aca voy a crear un hilo para q se quede esperando el contexto actualizado desde cpu
        pthread_t hilo_esperar_finalizacion_proceso;
        pthread_create(&hilo_esperar_finalizacion_proceso, NULL, atender_finalizacion_proceso, NULL);
    }
    else
        log_trace(logger, "No fue posible asignar un proceso al CPU.");
}

void evaluar_NEW_a_EXIT();
void evaluar_EXEC_a_READY();
void evaluar_READY_a_EXIT();
void evaluar_BLOCKED_a_EXIT();
void evaluar_EXEC_a_BLOCKED();
void evaluar_BLOCKED_a_READY();
void evaluar_EXEC_a_EXIT();

void mostrar_menu()
{
    log_info(logger, "\n\n|##########################|\n|     MENU DE OPCIONES     |\n|##########################|\n|  INICIAR_PROCESO [PATH]  |\n|      PROCESO_ESTADO      |\n|  FINALIZAR_PROCESO [PI]  |\n|   INICIAR_PLAFICACION    |\n|  DETENER_PLANIFICACION   |\n|  EJECUTAR_SCRIPT [PATH]  |\n| MULTIPROGRAMACION [VALOR]|\n|          SALIR           |\n|##########################|\n");
}

void *atender_finalizacion_proceso(void *arg)
{
    log_debug(logger, "Entre a un hilo para atender la finalizacion del proceso %d", (int)queue_peek(cola_RUNNING));

    // magia para atender la respuesta del CPU
}