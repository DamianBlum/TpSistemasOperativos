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
bool esta_planificacion_pausada = true;
// lista de pcbs
t_list *lista_de_pcbs;
// diccionario de recursos
t_dictionary *diccionario_recursos;
// Queues de estados
t_queue *cola_NEW;
t_queue *cola_READY;
t_queue *cola_RUNNING; // si, es una cola para un unico proceso, algun problema?
// t_queue *cola_BLOCKED; no se usa mas esto, ahora las colas de bloqueados estan dentro de diccionario_recursos
t_queue *cola_EXIT;              // esta es mas q nada para desp poder ver los procesos q ya terminaron
t_queue *cola_READY_PRIORITARIA; // es la q usa VRR para su wea
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
            // evaluar_NEW_a_READY(); creeeeeeo q esto no deberia estar aca
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
            evaluar_NEW_a_READY();
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

    if (crear_proceso_en_memoria(nuevo_pcb->processID, path))
    { // se creo el proceso correctamente en memoria
        list_add(lista_de_pcbs, nuevo_pcb);
        queue_push(cola_NEW, nuevo_pcb->processID);                              // ahora las colas solo van a tener los ids (para manejarlo mas simple)
        log_info(logger, "Se crea el proceso %d en NEW.", nuevo_pcb->processID); // log obligatorio
    }
    else
    { // no se pudo generar el proceso ya q el path esta mal
        log_error(logger, "No se pudo crear el proceso ya que el path ingresado no corresponde a ningun programa: %s", path);
    }
}

void eliminar_proceso(uint32_t id)
{
    // el llamado a liberar memoria se hace desde los evaluar_ALGO_a_ALGO

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
        evaluar_BLOCKED_a_EXIT(pcb_a_finalizar);
        break;
    default:
        break;
    }
}

bool eliminar_id_de_la_cola(t_queue *cola, uint32_t id) // si lo encontre y elimine, retorno true, si no false
{
    bool loEncontre = false;
    uint32_t primerId = queue_pop(cola);
    if (primerId != id)
    {
        queue_push(cola, primerId);
        while (queue_peek(cola) != primerId) // ya di toda la vuelta
        {
            uint32_t idActual = queue_pop(cola);
            if (idActual == id)
                loEncontre = true; // es el id q tengo q sacar
            else
                queue_push(cola, idActual);
        }
    }
    return loEncontre;
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
    cola_RUNNING = queue_create();
    cola_EXIT = queue_create();
    cola_READY_PRIORITARIA = queue_create();
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

    while (!esta_planificacion_pausada && !queue_is_empty(cola_NEW)) // agrego esta_planificacion_pausada para el caso borde donde creas un proceso nuevo con planificacion pausada
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

void evaluar_READY_a_EXEC() // hilar (me olvide xq xD)
{
    log_trace(logger, "Voy a evaluar si puedo mover un proceso de READY a EXEC (asignar un proceso al CPU).");
    if (queue_is_empty(cola_RUNNING) && !queue_is_empty(cola_READY) && !esta_planificacion_pausada) // valido q no este nadie corriendo, ready no este vacio y la planificacion no este pausada
    {                                                                                               // tengo q hacer algo distinto segun cada algoritmo de planificacion
        uint32_t id;

        switch (algoritmo_planificacion)
        {
        case FIFO: // saco el primero
            log_trace(logger, "Entre a planificacion por FIFO.");
            id = queue_pop(cola_READY);
            break;
        case RR: // igual q fifo pero creo el hilo con el quantum
            log_trace(logger, "Entre a planificacion por RR.");
            id = queue_pop(cola_READY);
            pthread_t prr;
            pthread_create(&prr, NULL, trigger_interrupcion_quantum, obtener_pcb_de_lista_por_id(id));
            break;
        case VRR: // esto lo q tiene de especial es q tengo q hacer un hilo mas q vaya contando el tiempo de ejecucion y una segunda cola de ready para los procesos prioritarios
            log_trace(logger, "Entre a planificacion por VRR.");
            if (!queue_is_empty(cola_READY_PRIORITARIA))
            {
                log_trace(logger, "Voy a elegir un proceso de la cola prioritaria.");
                id = queue_pop(cola_READY_PRIORITARIA);
            }
            else
            {
                log_trace(logger, "Voy a elegir un proceso de la cola normal.");
                id = queue_pop(cola_READY);
            }
            pthread_t pvrr;
            pthread_create(&pvrr, NULL, trigger_interrupcion_quantum, obtener_pcb_de_lista_por_id(id));
            // arranco el cronometro para q se ponga a contar el tiempo en ejecucion
            obtener_pcb_de_lista_por_id(id)->tiempo_en_ejecucion = temporal_create();
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
    log_trace(logger, "Voy a mover el proceso %u de NEW a EXIT", pcb->processID);
    // libero la memoria
    liberar_memoria(pcb->processID);
    // le cambio el estado
    pcb->estado = E_EXIT;
    // lo popeo de su cola actual
    eliminar_id_de_la_cola(cola_NEW, pcb->processID);
    // lo pusheo en exit
    queue_push(cola_EXIT, pcb->processID);
    // desminuyo el contador de procesos
    cant_procesos_ejecutando--;
    log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
}

void evaluar_EXEC_a_READY()
{
    log_trace(logger, "Voy a evaluar si puedo mover un proceso de EXEC a READY.");
    // estas validaciones las hago por las dudas nada mas, creo q en ningun caso se puede dar esto
    if (!queue_is_empty(cola_RUNNING))
    {

        t_PCB *pcb = devolver_pcb_desde_lista(lista_de_pcbs, (uint32_t)queue_pop(cola_RUNNING));
        // hago las cosas especificas de VRR
        if (debe_ir_a_cola_prioritaria(pcb)) // se fija si estoy en vrr y si tiene q ir a prio
        {
            pcb->estado = E_READY_PRIORITARIO;
            queue_push(cola_READY_PRIORITARIA, pcb->processID);
            log_trace(logger, "Se movio el proceso %d de EXEC a READY PRIORITARIO.", pcb->processID);
        }
        else
        {
            pcb->estado = E_READY;
            queue_push(cola_READY, pcb->processID);
            log_trace(logger, "Se movio el proceso %d de EXEC a READY.", pcb->processID);
        }
    }
    else
    {
        log_trace(logger, "No fue posible mover un proceso de EXEC a READY, por que no habia.");
    }
    evaluar_READY_a_EXEC(); // planifico xq se libero la cpu
}

void evaluar_READY_a_EXIT(t_PCB *pcb)
{
    log_trace(logger, "Voy a mover el proceso %d de READY a EXIT.", pcb->processID);
    // libero la memoria
    liberar_memoria(pcb->processID);
    // le cambio el estado
    pcb->estado = E_EXIT;
    // lo popeo de su cola actual
    eliminar_id_de_la_cola(cola_READY, pcb->processID);
    // lo pusheo en exit
    queue_push(cola_EXIT, pcb->processID);
    // desminuyo el contador de procesos
    cant_procesos_ejecutando--;
    log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
}

void evaluar_BLOCKED_a_EXIT(t_PCB *pcb)
{
    log_trace(logger, "Voy a mover el proceso %d de BLOCKED a EXIT.", pcb->processID);
    // libero la memoria
    liberar_memoria(pcb->processID);
    // le cambio el estado
    pcb->estado = E_EXIT;
    // lo popeo de su cola actual
    for (uint8_t i = 0; i < dictionary_size(diccionario_recursos); i++) // podria ser un poquito mas lindo esto pero bueno, andar anda
    {
        t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos, list_get(dictionary_keys(diccionario_recursos), i));
        if (eliminar_id_de_la_cola(tmb->cola_bloqueados, pcb->processID))
        {
            tmb->instancias_recursos++; // devulvo la instancia del recurso q se va a matar
            break;
        }
    }
    // lo pusheo en exit
    queue_push(cola_EXIT, pcb->processID);
    // desminuyo el contador de procesos
    cant_procesos_ejecutando--;
    log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
}

void evaluar_EXEC_a_BLOCKED(char *recurso)
{
    log_trace(logger, "Voy a evaluar si puedo mover un proceso de la cola EXEC a BLOCKED.");
    if (!queue_is_empty(cola_RUNNING))
    {
        t_PCB *pcb = devolver_pcb_desde_lista(lista_de_pcbs, (uint32_t)queue_pop(cola_RUNNING));
        pcb->estado = E_BLOCKED;

        // usando el recurso, consigo el t_manejo_bloqueados y meto el id en la cola, y disminuyo su contador
        t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos, recurso); // no valido q el recurso exista xq ya lo valide antes
        queue_push(tmb->cola_bloqueados, pcb->processID);

        log_trace(logger, "Se movio el proceso %d de EXEC a READY.", pcb->processID);
    }
    else
    {
        log_trace(logger, "No fue posible mover un proceso de EXEC a BLOCKED, por que no habia.");
    }
    evaluar_READY_a_EXEC(); // planifico xq se libero la cpu
}

void evaluar_BLOCKED_a_READY(t_queue *colaRecurso)
{ // desbloqueo por fifo
    log_trace(logger, "Voy a evaluar si puedo mover a algun proceso de la cola BLOCKED a READY.");

    if (queue_is_empty(colaRecurso))
    { // no hay nadie q desbloquear
        log_trace(logger, "No hay procesos bloqueados por el recurso.");
        return;
    }

    uint32_t id = queue_pop(colaRecurso);
    queue_push(cola_READY, id);

    t_PCB *pcb = devolver_pcb_desde_lista(lista_de_pcbs, id);

    // hago las cosas especificas de VRR
    if (debe_ir_a_cola_prioritaria(pcb)) // se fija si estoy en vrr y si tiene q ir a prio
    {
        log_trace(logger, "El proceso %d va a desbloquearse a la cola de READY PRIORITARIO.");
        pcb->estado = E_READY_PRIORITARIO;
        queue_push(cola_READY_PRIORITARIA, pcb->processID);
    }
    else
    { // entra aca si estoy en FIFO y RR
        log_trace(logger, "El proceso %d va a desbloquearse a la cola de READY.");
        pcb->estado = E_READY;
        queue_push(cola_READY, pcb->processID);
    }
}

void evaluar_EXEC_a_EXIT()
{
    uint32_t id = (uint32_t)queue_pop(cola_RUNNING);

    log_trace(logger, "Voy a mover el proceso %d de EXEC a EXIT.", id);

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

    // aca tengo q pausar la planificacion si me metieron un DETENER_PLANIFICACION
    // await(esta_planificacion_pausada)
    t_PCB *pcb_en_running;
    bool sigo_esperando_cosas_de_cpu = true;
    while (sigo_esperando_cosas_de_cpu)
    {
        int op = recibir_operacion(cliente_cpu_dispatch, logger); // si, uso el cliente como socket servidor

        if (op == PAQUETE)
        {
            t_list *lista_respuesta_cpu = list_create();
            lista_respuesta_cpu = recibir_paquete(cliente_cpu_dispatch, logger);
            log_trace(logger, "CPU me devolvio el contexto de ejecucion.");
            pcb_en_running = devolver_pcb_desde_lista(lista_de_pcbs, idProcesoActual);
            actualizar_pcb(lista_respuesta_cpu, pcb_en_running, logger);
            // ---------------------------------------------- //
            e_motivo_desalojo motivo_desalojo = conseguir_motivo_desalojo_de_registros_empaquetados(lista_respuesta_cpu);
            log_trace(logger, "Motivo de desalojo de %d: %s", pcb_en_running->processID, motivo_desalojo_texto(motivo_desalojo));
            switch (motivo_desalojo)
            {
            case MOTIVO_DESALOJO_EXIT:
                evaluar_EXEC_a_EXIT();
                // termino el ciclo
                sigo_esperando_cosas_de_cpu = false;
                break;
            case MOTIVO_DESALOJO_WAIT:
                char *argWait = list_get(lista_respuesta_cpu, 13); // hacer desp esto en una funcion
                log_debug(logger, "Argumento del wait: %s", argWait);
                t_paquete *respuesta_para_cpu = crear_paquete();
                // hago la magia de darle los recursos
                uint8_t resultado_asignar_recurso = asignar_recurso(argWait, pcb_en_running);
                if (resultado_asignar_recurso == 0)
                { // 0: te di la instancia
                    log_trace(logger, "Voy a enviarle al CPU que tiene la instancia.");
                    agregar_a_paquete(respuesta_para_cpu, 0, sizeof(uint8_t));
                }
                else if (resultado_asignar_recurso == 1)
                { // 1: no te la di (te bloqueo)
                    log_trace(logger, "Voy a enviarle al CPU que no tiene la instancia, asi q sera bloqueado.");
                    agregar_a_paquete(respuesta_para_cpu, 1, sizeof(uint8_t));
                    evaluar_EXEC_a_BLOCKED(argWait);
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                else if (resultado_asignar_recurso == 2)
                { // 2: mato al proceso xq pidio algo nada q ver
                    log_error(logger, "El cpu me pidio un recurso que no existe. Lo tenemos que matar!");
                    agregar_a_paquete(respuesta_para_cpu, 1, sizeof(uint8_t)); // le mando un 1 xq para cpu es lo mismo matar el proceso que bloquearlo
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                enviar_paquete(respuesta_para_cpu, cliente_cpu_dispatch, logger);
                break;
            case MOTIVO_DESALOJO_SIGNAL:
                char *argSignal = list_get(lista_respuesta_cpu, 13); // hacer desp esto en una funcion
                log_debug(logger, "Argumento del signal: %s", argSignal);
                t_paquete *respuesta_para_cpu_signal = crear_paquete();
                // desasigno
                uint8_t resultado_asignar_recurso_signal = desasignar_recurso(argSignal, pcb_en_running);
                if (resultado_asignar_recurso_signal == 0)
                { // hay instancias disponibles, voy a desbloquear a alguien y le respondo a cpu
                    evaluar_BLOCKED_a_READY((t_queue *)((t_manejo_bloqueados *)dictionary_get(diccionario_recursos, argSignal))->cola_bloqueados);
                    log_trace(logger, "Voy a enviarle al CPU que salio todo bien.");
                }
                else if (respuesta_para_cpu_signal == 1)
                { // le digo a cpu q desaloje el proceso y lo mando a exit
                    log_error(logger, "El cpu me pidio un recurso que no existe. Lo tenemos que matar!");
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                agregar_a_paquete(respuesta_para_cpu_signal, respuesta_para_cpu_signal, sizeof(uint8_t));
                enviar_paquete(respuesta_para_cpu_signal, cliente_cpu_dispatch, logger);
                break;
            case MOTIVO_DESALOJO_INTERRUPCION:
                // termino el ciclo
                sigo_esperando_cosas_de_cpu = false;

                // planifico
                evaluar_EXEC_a_READY();
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
        {
            log_error(logger, "Me mandaron cualquier cosa, voy a romper todo.");
            // termino el ciclo
            sigo_esperando_cosas_de_cpu = false;
        }
    }
    cosas_vrr_cuando_se_desaloja_un_proceso(pcb_en_running); // lo hago aca al final xq es cuando se q el proceso fue desalojado
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

uint8_t asignar_recurso(char *recurso, t_PCB *pcb)
{
    t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos, recurso);
    uint8_t r;
    if (tmb != NULL)
    { // existe, esta todo piola
        // hasta aca llegue (los logs de abajo no los hace)
        log_debug(logger, "Valor del recurso %s antes de modificarlo: %d", recurso, tmb->instancias_recursos);
        tmb->instancias_recursos -= 1;
        log_debug(logger, "Valor del recurso %s desp de modifiarlo: %d", recurso, tmb->instancias_recursos);

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

void asd(t_manejo_bloqueados *tmb)
{
    log_debug(logger, "-----------------------------------------------------");

    for (int i = 0; i < list_size(dictionary_keys(diccionario_recursos)); i++)
        log_debug(logger, "%d- Key: %s | Valor ir: %d", i, list_get(dictionary_keys(diccionario_recursos), i), ((t_manejo_bloqueados *)dictionary_get(diccionario_recursos, list_get(dictionary_keys(diccionario_recursos), i)))->instancias_recursos);

    log_debug(logger, "-----------------------------------------------------");
}

uint8_t desasignar_recurso(char *recurso, t_PCB *pcb)
{
    t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos, recurso);
    uint8_t r;
    if (tmb != NULL)
    {
        log_debug(logger, "Valor del recurso %s antes de modificarlo: %d", recurso, tmb->instancias_recursos);
        tmb->instancias_recursos += 1;
        log_debug(logger, "Valor del recurso %s desp de modifiarlo: %d", recurso, tmb->instancias_recursos);
        r = 0;
    }
    else
    { // cagaste
        r = 1;
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
    agregar_a_paquete(p, 1, sizeof(uint8_t)); // 1 es de borrar proceso
    agregar_a_paquete(p, id, sizeof(id));
    // enviar_paquete(p, cliente_memoria, logger); todavia no esta implementado en memoria
}

bool crear_proceso_en_memoria(uint32_t id, char *path)
{
    log_trace(logger, "Voy a enviarle a memoria el path para que cree el proceso.");
    t_paquete *p = crear_paquete();
    agregar_a_paquete(p, 0, sizeof(uint8_t)); // el 0 es de iniciar proceso
    agregar_a_paquete(p, path, strlen(path) + 1);
    agregar_a_paquete(p, id, sizeof(id));
    enviar_paquete(p, cliente_memoria, logger);

    // espero la respuesta sobre como salio la creacion
    bool v = false;
    int op = recibir_operacion(cliente_memoria, logger);
    if (op == PAQUETE)
    {
        t_list *l = list_create();
        l = recibir_paquete(cliente_memoria, logger);
        log_debug(logger, "Respuesta recibida de Memoria: %u", (uint8_t)list_get(l, 0));
        v = ((uint8_t)list_get(l, 0) == 0);
        list_destroy(l);
    }
    // recibir cualquier otra cosa implica false, por que? porque si
    return v;
}

// desarrollo del quantum

void *trigger_interrupcion_quantum(void *args) // escuchar audio q me mande a wsp
{
    t_PCB *pcb = (t_PCB *)args;
    log_trace(logger, "Entre al hilo para enviar la interrupcion de fin de quantum del proceso %u.", pcb->processID);
    log_debug(logger, "Valor del quantum: %u", pcb->quantum); // el %u es para unsigned, x las dudas

    sleep(pcb->quantum / 1000); // seteo el q de ms a s

    // el objetivo de esto es saber si el proceso sigue ejecutando
    // imaginate q yo creo el proceso y, por lo tanto, esta funcion. Desp el proceso se desaloja por alguna razon sin terminar su quantum
    // entonces reviso si el proceso en cuestion sigue ejecutando y, si es asi, mando la interrupcion, el problema de esto es:
    // como carajo consigo el quantum q me falto ejecutar? opcion q se me ocurrio, HAY OTRO HILO QUE LLEVA EL TIEMPO EJECUTANDO (NOSECOMO) Y LO DEVUELVE (TAMPOCOSECOMO) Y DESP LOS RESTO

    log_trace(logger, "Termino el quantum del proceso %u.", pcb->processID);

    if (!queue_is_empty(cola_RUNNING) && queue_peek(cola_RUNNING) == pcb->processID) // lo primero es xq si es el unico proceso en el sistema, voy a tener un sf haciendo el peek
    {
        log_trace(logger, "Voy a mandar la interrupcion a CPU para el proceso %u.", pcb->processID);
        t_paquete *paquete_interrupcion = crear_paquete();
        agregar_a_paquete(paquete_interrupcion, pcb->processID, sizeof(pcb->processID)); // le mando el id del proceso xq no se q mandar
        enviar_paquete(paquete_interrupcion, cliente_cpu_interrupt, logger);
    }
    else
    {
        log_trace(logger, "El proceso %u fue desalojado del CPU antes de que terminara su queantum, no mando nada.", pcb->processID);
    }

    log_trace(logger, "Termino el hilo para el queantum del proceso %u.", pcb->processID);
}

void cosas_vrr_cuando_se_desaloja_un_proceso(t_PCB *pcb)
{ // si estoy en vrr, dentengo el cronometro y le resto al quantum lo ejecutado para quedarme con lo q le falta
    if (algoritmo_planificacion == VRR)
    {
        temporal_stop(pcb->tiempo_en_ejecucion); // esto en si no es necesario pero bueno, por las dudas lo detengo
        int64_t tiempo_ejecutado = temporal_gettime(pcb->tiempo_en_ejecucion);
        log_debug(logger, "Tiempo que se ejecuto el programa: %d | Quantum: %d", tiempo_ejecutado, quantum);

        temporal_destroy(pcb->tiempo_en_ejecucion); // esto es para evitar problemas de memoria, cada vez q paso un proceso de algo a exec, lo creo de vuelta
        if (tiempo_ejecutado < pcb->quantum)
        {                                     // teoricamente, esto es siempre verdad salvo en el caso de q mande una interrupcion al cpu por fin de quantum, por eso hago esta validacion
            pcb->quantum -= tiempo_ejecutado; // obtengo mi nuevo quantum
        }
        else
        { // reinicio el quantum para la prox vez q tenga q ejecutar
            pcb->quantum = quantum;
        }

        log_debug(logger, "Se modifico el quantum del proceso %u a %u para la planificacion por VRR.", pcb->processID, pcb->quantum);
    }
}

bool debe_ir_a_cola_prioritaria(t_PCB *pcb)
{
    return pcb->quantum < quantum && algoritmo_planificacion == VRR;
}
