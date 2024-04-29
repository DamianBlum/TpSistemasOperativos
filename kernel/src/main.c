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
int contadorParaGenerarIdProcesos = 0;
int quantum;
int grado_multiprogramacion;  // este es el definido por parametro de config
int cant_procesos_ejecutando; // y este es un contador de procesos en el sistema, se modifica en el planificador de largo plazo
e_algoritmo_planificacion algoritmo_planificacion;
bool esta_planificacion_pausada = false;
// lista de pcbs
t_list *lista_de_pcbs;
// diccionario de recursos
t_dictionary *diccionario_recursos;
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
    obtener_valores_de_recursos();

    // PARTE CLIENTE
    if (generar_clientes()) // error al crear los clientes de cpu
        return EXIT_FAILURE;

    // CREACION HILO SERVIDOR I/O
    /*
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
            eliminar_proceso(atoi(comandoSpliteado[1]));
            evaluar_READY_a_EXEC();
            evaluar_NEW_a_READY();
        }
        else if (string_equals_ignore_case("INICIAR_PLAFICACION", comandoSpliteado[0]) || string_equals_ignore_case("IPL", comandoSpliteado[0]))
        {
            log_debug(logger, "Entraste a INICIAR_PLAFICACION.");
            esta_planificacion_pausada = false;
            evaluar_READY_a_EXEC(); // este esta para cuando recien se arranco el programa y cuando se despausa la planificacion
        }
        else if (string_equals_ignore_case("DETENER_PLANIFICACION", comandoSpliteado[0]) || string_equals_ignore_case("DP", comandoSpliteado[0]))
        {
            log_debug(logger, "Entraste a DETENER_PLANIFICACION.");
            esta_planificacion_pausada = true;
        }
        else if (string_equals_ignore_case("EJECUTAR_SCRIPT", comandoSpliteado[0]) || string_equals_ignore_case("ES", comandoSpliteado[0]) && comandoSpliteado[1] != NULL)
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

    t_PCB *nuevo_pcb = crear_pcb(&contadorParaGenerarIdProcesos, quantum);

    list_add(lista_de_pcbs, nuevo_pcb);

    queue_push(cola_NEW, nuevo_pcb->processID); // ahora las colas solo van a tener los ids (para manejarlo mas simple)

    log_info(logger, "Se crea el proceso %d en NEW.", nuevo_pcb->processID); // log obligatorio
}

void eliminar_proceso(uint32_t id)
{
    // tengo q decirle a memoria q libere lo de este proceso

    t_PCB *pcb_a_finalizar = devolver_pcb_desde_lista(lista_de_pcbs, id);
    if (pcb_a_finalizar == NULL)
    {
        log_error(logger, "El proceso %d no existe.");
        return;
    }
    e_estado_proceso estado = pcb_a_finalizar->estado;

    // lo saco de queue
    switch (estado)
    {
    case E_NEW:
        evaluar_NEW_a_EXIT(pcb_a_finalizar);
        break;
    case E_READY:
        evaluar_READY_a_EXIT(pcb_a_finalizar);
        break;
    case E_RUNNING:
        // primero mando interrupcion a cpu
        t_paquete *paquete_interrupt = crear_paquete();
        // creo q no importa mucho q es lo q mande, sino simplemente mandar algo
        agregar_a_paquete(paquete_interrupt, (uint32_t)1, sizeof(uint32_t));
        enviar_paquete(paquete_interrupt, cliente_cpu_interrupt, logger);
        // planifico
        evaluar_EXEC_a_EXIT();
        break;
    case E_BLOCKED:
        evaluar_BLOCKED_a_EXIT();
        break;
    default:
        break;
    }
}

void eliminar_id_de_la_cola(t_queue *cola, uint32_t id)
{
    uint32_t primerId = queue_pop(cola);
    if (primerId != id)
    {
        queue_push(cola, primerId);
        while (queue_peek(cola) != primerId) // ya di toda la vuelta
        {
            uint32_t idActual = queue_pop(cola);
            if (idActual == id)
                break; // es el id q tengo q sacar
            queue_push(cola, idActual);
        }
    }
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
    // cola_BLOCKED = queue_create(); estas colas ahora se instancias en el t_manejo_bloqueados *crear_manejo_bloqueados();
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
        if (cant_procesos_ejecutando < grado_multiprogramacion)
        {
            uint32_t id = queue_pop(cola_NEW);

            queue_push(cola_READY, id);

            t_PCB *pcb_elegido = obtener_pcb_de_lista_por_id(id);
            pcb_elegido->estado = E_READY;

            log_trace(logger, "Fue posible mover el proceso %d de NEW a READY.", id);

            cant_procesos_ejecutando++;
            log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
        }
        else
        {
            log_trace(logger, "No puedo porque el grado de multiprogramacion (%d) no admite mas programas en el sistema (actual: %d).", grado_multiprogramacion, cant_procesos_ejecutando);
            break;
        }
    }
}

void evaluar_READY_a_EXEC() // hilar
{
    log_trace(logger, "Entre a READY a EXEC para evaluar si se puede asignar un proceso al CPU.");
    if (queue_is_empty(cola_RUNNING) && !queue_is_empty(cola_READY) && !esta_planificacion_pausada) // valido q no este nadie corriendo, ready no este vacio y la planificacion no este pausada
    {                                                                                               // tengo q hacer algo distinto segun cada algoritmo de planificacion
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
        t_PCB *pcb_elegido = obtener_pcb_de_lista_por_id(id);
        pcb_elegido->estado = E_RUNNING;

        // ya q estamos le mando a cpu el contexto de ejecucion del proceso elegido
        t_paquete *paquete_con_pcb = crear_paquete();
        empaquetar_pcb(paquete_con_pcb, pcb_elegido);
        enviar_paquete(paquete_con_pcb, cliente_cpu_dispatch, logger);

        // aca voy a crear un hilo para q se quede esperando el contexto actualizado desde cpu
        pthread_t hilo_esperar_finalizacion_proceso;
        pthread_create(&hilo_esperar_finalizacion_proceso, NULL, atender_respuesta_proceso, NULL);
    }
    else
    {
        log_trace(logger, "No fue posible asignar un proceso al CPU.");
        log_debug(logger, "Cant. procesos en READY: %d | Cant. procesos en RUNNING: %d", queue_size(cola_READY), queue_size(cola_RUNNING));
    }
}

void evaluar_NEW_a_EXIT(t_PCB *pcb)
{
    // libero la memoria
    liberar_memoria(pcb->processID);
    // le cambio el estado
    pcb->estado = E_EXIT;
    // lo popeo de su cola actual
    eliminar_id_de_la_cola(cola_NEW, pcb->processID);
    // lo pusheo en exit
    queue_push(cola_EXIT, pcb_a_finalizar->processID);
    // desminuyo el contador de procesos
    cant_procesos_ejecutando--;
    log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
}
void evaluar_EXEC_a_READY()
{
    evaluar_READY_a_EXEC();
}
void evaluar_READY_a_EXIT(t_PCB *pcb)
{
    // libero la memoria
    liberar_memoria(pcb->processID);
    // le cambio el estado
    pcb->estado = E_EXIT;
    // lo popeo de su cola actual
    eliminar_id_de_la_cola(cola_READY, pcb->processID);
    // lo pusheo en exit
    queue_push(cola_EXIT, pcb_a_finalizar->processID);
    // desminuyo el contador de procesos
    cant_procesos_ejecutando--;
    log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
}
void evaluar_BLOCKED_a_EXIT(t_PCB *pcb)
{
    // libero la memoria
    liberar_memoria(pcb->processID);
    // le cambio el estado
    pcb->estado = E_EXIT;
    // lo popeo de su cola actual
    for (uint8_t i = 0; i < dictionary_size(diccionario_recursos); i++) // podria ser un poquito mas lindo esto pero bueno, andar anda
    {
        t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos, list_get(dictionary_keys(diccionario_recursos), i));
        eliminar_id_de_la_cola(tmb->cola_bloqueados, pcb_a_finalizar->processID);
    }
    // lo pusheo en exit
    queue_push(cola_EXIT, pcb_a_finalizar->processID);
    // desminuyo el contador de procesos
    cant_procesos_ejecutando--;
    log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
}
void evaluar_EXEC_a_BLOCKED()
{
    evaluar_READY_a_EXEC();
}
void evaluar_BLOCKED_a_READY(t_queue colaRecurso)
{ // desbloqueo por fifo
    uint32_t id = queue_pop(colaRecurso);
    queue_push(cola_READY, id);

    t_PCB *pcb = devolver_pcb_desde_lista(lista_de_pcbs, id);
    pcb->estado = E_READY;
}
void evaluar_EXEC_a_EXIT()
{
    // medio falso el nombre este xq no evaluo nada, simplemente hago todo lo necesario para terminar el proceso
    uint32_t id = (uint32_t)queue_pop(cola_RUNNING);

    liberar_memoria(id);

    t_PCB *pcb_en_running = devolver_pcb_desde_lista(lista_de_pcbs, id);
    pcb_en_running->estado = E_EXIT;
    queue_push(cola_EXIT, id);
    cant_procesos_ejecutando--;
    log_trace(logger, "Se movio el proceso %d a EXIT. Grado de multiprogramacion actual: %d.", id, cant_procesos_ejecutando);
    // los pongo en este orden segun el tipo de planificadores q son
    evaluar_READY_a_EXEC();
    evaluar_NEW_a_READY();
}

void mostrar_menu()
{
    if (logger->is_active_console)
        log_info(logger, "\n\n|##########################|\n|     MENU DE OPCIONES     |\n|##########################|\n|  INICIAR_PROCESO [PATH]  |\n|      PROCESO_ESTADO      |\n|  FINALIZAR_PROCESO [PI]  |\n|   INICIAR_PLAFICACION    |\n|  DETENER_PLANIFICACION   |\n|  EJECUTAR_SCRIPT [PATH]  |\n| MULTIPROGRAMACION [VALOR]|\n|          SALIR           |\n|##########################|\n");
    else
        printf("\n\n|##########################|\n|     MENU DE OPCIONES     |\n|##########################|\n|  INICIAR_PROCESO [PATH]  |\n|      PROCESO_ESTADO      |\n|  FINALIZAR_PROCESO [PI]  |\n|   INICIAR_PLAFICACION    |\n|  DETENER_PLANIFICACION   |\n|  EJECUTAR_SCRIPT [PATH]  |\n| MULTIPROGRAMACION [VALOR]|\n|          SALIR           |\n|##########################|\n");
}

void *atender_respuesta_proceso(void *arg)
{

    uint32_t idProcesoActual = queue_peek(cola_RUNNING); // esto es para el log del final
    log_trace(logger, "Entre a un hilo para atender la finalizacion del proceso %d.", idProcesoActual);

    int op = recibir_operacion(cliente_cpu_dispatch, logger); // si, uso el cliente como socket servidor
    // aca tengo q pausar la planificacion si me metieron un DETENER_PLANIFICACION
    // await(esta_planificacion_pausada)

    if (op == PAQUETE)
    {
        t_list *lista_respuesta_cpu = list_create();
        lista_respuesta_cpu = recibir_paquete(cliente_cpu_dispatch, logger);
        log_trace(logger, "CPU me devolvio el contexto de ejecucion.");
        t_PCB *pcb_en_running = devolver_pcb_desde_lista(lista_de_pcbs, idProcesoActual);
        actualizar_pcb(lista_respuesta_cpu, pcb_en_running, logger);
        // ---------------------------------------------- //
        e_motivo_desalojo motivo_desalojo = conseguir_motivo_desalojo_de_registros_empaquetados(lista_respuesta_cpu);
        switch (motivo_desalojo)
        {
        case MOTIVO_DESALOJO_EXIT:
            evaluar_EXEC_a_EXIT();
            break;
        case MOTIVO_DESALOJO_WAIT:
            char *argWait = list_get(lista_respuesta_cpu, 14); // hacer desp esto en una funcion
            log_debug(logger, "Argumento del wait: %s", argWait);
            t_paquete *respuesta_para_cpu = crear_paquete();
            // hago la magia de darle los recursos
            uint8_t resultado_asignar_recurso = asignar_recurso(argWait);
            if (resultado_asignar_recurso == 0)
            { // 0: te di la instancia
                log_trace(logger, "Voy a enviarle al CPU que tiene la instancia.");
                agregar_a_paquete(respuesta_para_cpu, 0, sizeof(uint8_t));
                // hacer un while de todo y matarlo cuando se tenga q matar el hilo :D
            }
            else if (resultado_asignar_recurso == 1)
            { // 1: no te la di (te bloqueo)
                log_trace(logger, "Voy a enviarle al CPU que no tiene la instancia, asi q sera bloqueado.");
                agregar_a_paquete(respuesta_para_cpu, 1, sizeof(uint8_t));
                evaluar_EXEC_a_BLOCKED();
            }
            else if (resultado_asignar_recurso == 2)
            { // 2: mato al proceso xq pidio algo nada q ver
                log_error(logger, "El cpu me pidio un recurso que no existe. Lo tenemos que matar!");
                agregar_a_paquete(respuesta_para_cpu, 1, sizeof(uint8_t)); // le mando un 1 xq para cpu es lo mismo matar el proceso que bloquearlo
                evaluar_EXEC_a_EXIT();
            }
            enviar_paquete(respuesta_para_cpu, cliente_cpu_dispatch, logger);
            break;
        case MOTIVO_DESALOJO_SIGNAL:
            char *argSignal = list_get(lista_respuesta_cpu, 14); // hacer desp esto en una funcion
            log_debug(logger, "Argumento del signal: %s", argWait);
            t_paquete *respuesta_para_cpu_signal = crear_paquete();
            // desasigno
            uint8_t resultado_asignar_recurso_signal = desasignar_recurso(argSignal);
            if (resultado_asignar_recurso_signal == 0)
            { // hay instancias disponibles, voy a desbloquear a alguien y le respondo a cpu
                evaluar_BLOCKED_a_READY(((t_manejo_bloqueados *)dictionary_get(diccionario_recursos, argSignal))->cola_bloqueados);
                log_trace(logger, "Voy a enviarle al CPU que salio todo bien.");
                agregar_a_paquete(respuesta_para_cpu_signal, 0, sizeof(uint8_t));
            }
            else if (resultado_asignar_recurso_signal == 1)
            { // solamente le digo a cpu q esta todo bien
                log_trace(logger, "Voy a enviarle al CPU que salio todo bien. Pero no se desbloquea ningun proceso.");
                agregar_a_paquete(respuesta_para_cpu_signal, 0, sizeof(uint8_t));
            }
            else if (respuesta_para_cpu_signal == 2)
            { // le digo a cpu q desaloje el proceso y lo mando a exit
                log_error(logger, "El cpu me pidio un recurso que no existe. Lo tenemos que matar!");
                agregar_a_paquete(respuesta_para_cpu_signal, 1, sizeof(uint8_t)); // le mando un 1 xq para cpu es lo mismo matar el proceso que bloquearlo
                evaluar_EXEC_a_EXIT();
            }
            break;
        case MOTIVO_DESALOJO_INTERRUPCION:
            break;
        case MOTIVO_DESALOJO_IO_GEN_SLEEP:
            break;
        case MOTIVO_DESALOJO_IO_STDIN_READ:
            break;
        case MOTIVO_DESALOJO_IO_STDOUT_WRITE:
            break;
        case MOTIVO_DESALOJO_IO_FS_CREATE:
            break;
        case MOTIVO_DESALOJO_IO_FS_DELETE:
            break;
        case MOTIVO_DESALOJO_IO_FS_TRUNCATE:
            break;
        case MOTIVO_DESALOJO_IO_FS_WRITE:
            break;
        case MOTIVO_DESALOJO_IO_FS_READ:
            break;
        default:
            log_error(logger, "Recibi cualquier cosa como motivo de desalojo");
            break;
        }
    }
    else
        log_error(logger, "Me mandaron cualquier cosa, voy a romper todo.");
    log_trace(logger, "Termino el hilo para el proceso %d.", idProcesoActual);
}

void obtener_valores_de_recursos()
{
    diccionario_recursos = dictionary_create();
    char **lista1 = config_get_array_value(config, "RECURSOS");
    char **lista2 = config_get_array_value(config, "INSTANCIAS_RECURSOS");

    int i = 0;
    while (true)
    {
        if ((lista1)[i] == NULL)
            break;
        t_manejo_bloqueados *tmb = crear_manejo_bloqueados();
        tmb->instancias_recursos = atoi(lista2[i]);
        dictionary_put(diccionario_recursos, (lista1)[i], tmb);
        i++;
    }
}

uint8_t asignar_recurso(char *recurso)
{
    t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos, recurso);
    uint8_t r;
    if (tmb != NULL)
    { // existe, esta todo piola
        log_debug(logger, "Valor del recurso %s antes de modificarlo: %d", tmb->instancias_recursos);
        tmb->instancias_recursos -= 1;
        log_debug(logger, "Valor del recurso %s desp de modifiarlo: %d", tmb->instancias_recursos);
        if (tmb->instancias_recursos < 0)
            r = 1; // hay q bloquear el proceso
        else
            r = 0; // lo devuelvo sin bloquear
    }
    else
    { // cagaste
        r = 2;
    }
    return r;
}

uint8_t desasignar_recurso(char *recurso)
{
    t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos, recurso);
    uint8_t r;
    if (tmb != NULL)
    { // existe, esta todo piola
        log_debug(logger, "Valor del recurso %s antes de modificarlo: %d", tmb->instancias_recursos);
        tmb->instancias_recursos += 1;
        log_debug(logger, "Valor del recurso %s desp de modifiarlo: %d", tmb->instancias_recursos);
        if (tmb->instancias_recursos >= 0)
            r = 0; // hay q desbloquear a alguien
        else
            r = 1; // creo q no hago nada?
    }
    else
    { // cagaste
        r = 2;
    }
    return r;
}

t_manejo_bloqueados *crear_manejo_bloqueados()
{
    t_manejo_bloqueados *tmb = malloc(sizeof(t_manejo_bloqueados));
    tmb->cola_bloqueados = queue_create();
    tmb->instancias_recursos = 0;
    return tmb;
}

void destruir_manejor_bloqueados(t_manejo_bloqueados *tmb)
{
    queue_destroy(tmb->cola_bloqueados);
    free(tmb);
}

// manejo de memoria

void liberar_memoria(uint32_t id)
{
    log_trace(logger, "Voy a decirle a memoria q libere lo del proceso %d", id);

    t_paquete *p = crear_paquete();
    // agregar_a_paquete(p, E_LIBERAR_MEMORIA, sizeof(e_acciones_memoria));
    agregar_a_paquete(p, id, sizeof(id));
    enviar_paquete(p, cliente_memoria, logger);
}

void crear_proceso_en_memoria(uint32_t id, char *path)
{
    t_paquete *p = crear_paquete();
    // agregar_a_paquete(p, E_CREAR_PROCESO_MEMORIA, sizeof(e_acciones_memoria));
    agregar_a_paquete(p, id, sizeof(id));
    agregar_a_paquete(p, path, strlen(path);
    enviar_paquete(p, cliente_memoria, logger);
    // entiendo q no es necesario devolver si esto salio bien o no
}