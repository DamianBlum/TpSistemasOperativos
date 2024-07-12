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
int grado_multiprogramacion; // este es el definido por parametro de config

// Variable para el mutex del grado_multiprogramacion
pthread_mutex_t mutex_grado_multiprogramacion = PTHREAD_MUTEX_INITIALIZER;
// Variable para el mutex de la cola de new
pthread_mutex_t mutex_cola_ready = PTHREAD_MUTEX_INITIALIZER;
int cant_procesos_ejecutando; // y este es un contador de procesos en el sistema, se modifica en el planificador de largo plazo

e_algoritmo_planificacion algoritmo_planificacion;

bool esta_planificacion_pausada = true;

// lista de pcbs
t_list *lista_de_pcbs;
// lista de i/os
// t_list *lista_de_entradas_salidas;
// diccionario de recursos
t_dictionary *diccionario_recursos_e_interfaces;
// Queues de estados
t_queue *cola_NEW;
t_queue *cola_READY;
t_queue *cola_RUNNING; // si, es una cola para un unico proceso, algun problema?
// t_queue *cola_BLOCKED; no se usa mas esto, ahora las colas de bloqueados estan dentro de diccionario_recursos_e_interfaces
t_queue *cola_EXIT;              // esta es mas q nada para desp poder ver los procesos q ya terminaron
t_queue *cola_READY_PRIORITARIA; // es la q usa VRR para su wea
// Queue de vrr
// esto se hara en su respectivo momento

int main(int argc, char *argv[])
{
    logger = iniciar_logger("kernel.log", "KERNEL", argc, argv);
    config = iniciar_config("kernel.config");
    pthread_mutex_init(&mutex_grado_multiprogramacion, NULL);
    pthread_mutex_init(&mutex_cola_ready, NULL);
    // Instancio variables de configuracion desde el config
    quantum = config_get_int_value(config, "QUANTUM");
    grado_multiprogramacion = config_get_int_value(config, "GRADO_MULTIPROGRAMACION");
    algoritmo_planificacion = obtener_algoritmo_planificacion(config_get_string_value(config, "ALGORITMO_PLANIFICACION"));
    lista_de_pcbs = list_create();
    // lista_de_entradas_salidas = list_create();
    instanciar_colas();
    obtener_valores_de_recursos();
    // obtener_valores_de_recursos();
    // diccionario_interfaz = dictionary_create(); tengo que ver si hay que crear o no esto, ver donde van las e-s :/
    //  PARTE CLIENTE
    if (generar_clientes()) // error al crear los clientes de cpu
        return EXIT_FAILURE;

    // CREACION HILO SERVIDOR I/O

    pthread_create(&hilo_servidor_io, NULL, atender_servidor_io, NULL);
    // pthread_join(hilo_servidor_io, NULL);

    // PARTE CONSOLA INTERACTIVA
    int seguir = 1;
    while (seguir)
    {
        mostrar_menu();
        char *comandoLeido;
        comandoLeido = readline(">"); // Lee de consola lo ingresado

        seguir = ejecutar_comando(comandoLeido);
        free(comandoLeido);
    }

    finalizar_kernel();

    return EXIT_SUCCESS;
}

uint8_t ejecutar_comando(char *comando)
{
    char **comandoSpliteado = string_split(comando, " ");
    uint8_t seguir = 1;

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
            char *estado_texto = estado_proceso_texto(p->estado);
            log_info(logger, "ID: %d Estado: %s", p->processID, estado_texto);
            log_debug(logger, "ID: %d PC: %d Q: %d Estado: %s AX: %d BX: %d CX: %d DX: %d EAX: %d EBX: %d ECX: %d EDX SI: %d DI: %d", p->processID, p->programCounter, p->quantum, estado_texto, p->AX, p->BX, p->CX, p->DX, p->EAX, p->EBX, p->ECX, p->EDX, p->SI, p->DI);
            free(estado_texto);
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

        // armamos el path
        char *path = string_duplicate("scripts_kernel/");
        string_append(&path, comandoSpliteado[1]);

        // leer archivo en cuestion
        FILE *file = fopen(path, "r");
        if (file == NULL)
        {
            log_error(logger, "No se encontro el script \"%s\".", path);
            return 1;
        }

        struct stat stat_file;
        stat(path, &stat_file);

        char *buffer = calloc(1, stat_file.st_size + 1);
        fread(buffer, stat_file.st_size, 1, file);
        fclose(file);

        char **lineas_de_comando = string_split(buffer, "\n");

        // ejecutar todos sus comandos

        string_iterate_lines(lineas_de_comando, ejecutar_comando);

        // matar todo
        string_array_destroy(lineas_de_comando);
        free(path);
        free(buffer);
    }
    else if ((string_equals_ignore_case("MULTIPROGRAMACION", comandoSpliteado[0]) || string_equals_ignore_case("MP", comandoSpliteado[0])) && comandoSpliteado[1] != NULL)
    {
        log_debug(logger, "Entraste a MULTIPROGRAMACION, nuevo grado: %s.", comandoSpliteado[1]);
        log_debug(logger, "Se va a modificar el grado de multiprogramacion de %d a %s.", grado_multiprogramacion, comandoSpliteado[1]);
        pthread_mutex_lock(&mutex_grado_multiprogramacion);
        grado_multiprogramacion = atoi(comandoSpliteado[1]);
        pthread_mutex_unlock(&mutex_grado_multiprogramacion);
        evaluar_NEW_a_READY();
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
    string_array_destroy(comandoSpliteado);
    return seguir;
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
    // Este servidor solamente se encarga de conectar los clientes de io y crear los hilos que los van a atender
    socket_servidor = iniciar_servidor(config, "PUERTO_ESCUCHA");

    log_debug(logger, "Servidor %d creado.", socket_servidor);

    while (true)
    {
        socket_cliente_io = esperar_cliente(socket_servidor, logger);

        pthread_t hilo_atender_cliente_io;
        pthread_create(&hilo_atender_cliente_io, NULL, atender_cliente_io, socket_cliente_io);
    }
}

void *atender_cliente_io(void *arg)
{
    int cliente_io = (int)arg;
    log_trace(logger, "Entre al hilo para atender al cliente de I/O %d", cliente_io);

    // espero el mensaje de IO donde me trae el nombre de su interfaz
    recibir_operacion(cliente_io, logger);
    char *nombreInterfaz = recibir_mensaje(cliente_io, logger);

    // valido que el nombre ya no este ingresado por otra interfaz
    t_paquete *resp = crear_paquete();
    if (dictionary_has_key(diccionario_recursos_e_interfaces, nombreInterfaz))
    {
        log_error(logger, "Servidor %d: La interfaz solicitada ya habia sido creada, voy a avisarle al cliente y finalizar este hilo.", cliente_io);
        agregar_a_paquete(resp, 1, sizeof(uint8_t)); // no se pudo crear
        enviar_paquete(resp, cliente_io, logger);
        return EXIT_FAILURE;
    }

    crear_interfaz(nombreInterfaz, cliente_io);
    agregar_a_paquete(resp, 0, sizeof(uint8_t)); // interfaz creada en kernel con exito
    enviar_paquete(resp, cliente_io, logger);

    // una vez creada la interfaz, uso este hilo para manejar la comunicacion con IO
    t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos_e_interfaces, nombreInterfaz);
    t_entrada_salida *tes = (t_entrada_salida *)tmb->datos_bloqueados;
    while (1)
    {
        // hago aca un wait para que no me joda mucho en la consola
        pthread_mutex_lock(&(tes->binario));
        log_trace(logger, "Pude pasar por el binario de la interfaz %s", tes->nombre_interfaz);

        wait_interfaz(tes);
        uint8_t hay_algo = !queue_is_empty(tmb->cola_bloqueados);
        signal_interfaz(tes);

        if (hay_algo)
        {
            wait_interfaz(tes);
            t_pid_con_datos *pid_con_datos = queue_peek(tmb->cola_bloqueados);
            signal_interfaz(tes);

            // saco los datos
            t_list *lista_de_parametros = (t_list *)pid_con_datos->datos;

            t_paquete *paquete_para_io = crear_paquete();

            // armo la lista de datos q le voy a enviar a la interfaz
            switch (pid_con_datos->tipo_parametros_io)
            {
            case GENERICA:
                uint32_t cant = list_get(lista_de_parametros, 0);

                agregar_a_paquete(paquete_para_io, "IO_GEN_SLEEP", strlen("IO_GEN_SLEEP") + 1);
                agregar_a_paquete(paquete_para_io, (uint32_t)list_get(lista_de_parametros, 1), sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, cant, sizeof(uint32_t));

                break;
            case STDIN:
                uint32_t dir_stdin = list_get(lista_de_parametros, 0);
                uint32_t size_reg_stdin = list_get(lista_de_parametros, 1);

                agregar_a_paquete(paquete_para_io, "IO_STDIN_READ", strlen("IO_STDIN_READ") + 1);
                agregar_a_paquete(paquete_para_io, (uint32_t)list_get(lista_de_parametros, 2), sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, dir_stdin, sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, size_reg_stdin, sizeof(uint32_t));
                break;
            case STDOUT:
                uint32_t dir_stdout = list_get(lista_de_parametros, 0);
                uint32_t size_reg_stdout = list_get(lista_de_parametros, 1);

                agregar_a_paquete(paquete_para_io, "IO_STDOUT_WRITE", strlen("IO_STDOUT_WRITE") + 1);
                agregar_a_paquete(paquete_para_io, (uint32_t)list_get(lista_de_parametros, 2), sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, dir_stdout, sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, size_reg_stdout, sizeof(uint32_t));

                break;
            case DIALFS_CREATE:
                char *nombre_archivo_c = list_get(lista_de_parametros, 0);
                agregar_a_paquete(paquete_para_io, "IO_FS_CREATE", strlen("IO_FS_CREATE") + 1);
                agregar_a_paquete(paquete_para_io, (uint32_t)list_get(lista_de_parametros, 1), sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, nombre_archivo_c, strlen(nombre_archivo_c) + 1);
                free(nombre_archivo_c);
                break;
            case DIALFS_DELETE:
                char *nombre_archivo_d = list_get(lista_de_parametros, 0);
                agregar_a_paquete(paquete_para_io, "IO_FS_DELETE", strlen("IO_FS_DELETE") + 1);
                agregar_a_paquete(paquete_para_io, (uint32_t)list_get(lista_de_parametros, 0), sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, nombre_archivo_d, strlen(nombre_archivo_d) + 1);
                free(nombre_archivo_d);
                break;
            case DIALFS_TRUNCATE:
                char *nombre_archivo_t = list_get(lista_de_parametros, 0);
                uint32_t tamanio_t = list_get(lista_de_parametros, 2);
                agregar_a_paquete(paquete_para_io, "IO_FS_TRUNCATE", strlen("IO_FS_TRUNCATE") + 1);
                agregar_a_paquete(paquete_para_io, (uint32_t)list_get(lista_de_parametros, 1), sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, nombre_archivo_t, strlen(nombre_archivo_t) + 1);
                agregar_a_paquete(paquete_para_io, tamanio_t, sizeof(tamanio_t));
                free(nombre_archivo_c);
                break;
            case DIALFS_READ:
                char *nombre_archivo_r = list_get(lista_de_parametros, 0);
                uint32_t df_r = list_get(lista_de_parametros, 2);
                uint32_t tamanio_r = list_get(lista_de_parametros, 3);
                uint32_t puntero_archivo_r = list_get(lista_de_parametros, 4);
                agregar_a_paquete(paquete_para_io, "IO_FS_READ", strlen("IO_FS_READ") + 1);
                agregar_a_paquete(paquete_para_io, (uint32_t)list_get(lista_de_parametros, 1), sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, nombre_archivo_r, strlen(nombre_archivo_r) + 1);
                agregar_a_paquete(paquete_para_io, df_r, sizeof(df_r));
                agregar_a_paquete(paquete_para_io, tamanio_r, sizeof(tamanio_r));
                agregar_a_paquete(paquete_para_io, puntero_archivo_r, sizeof(puntero_archivo_r));
                free(nombre_archivo_r);

                break;
            case DIALFS_WRITE:
                char *nombre_archivo_w = list_get(lista_de_parametros, 0);
                uint32_t df_w = list_get(lista_de_parametros, 2);
                uint32_t tamanio_w = list_get(lista_de_parametros, 3);
                uint32_t puntero_archivo_w = list_get(lista_de_parametros, 4);
                agregar_a_paquete(paquete_para_io, "IO_FS_WRITE", strlen("IO_FS_WRITE") + 1);
                agregar_a_paquete(paquete_para_io, (uint32_t)list_get(lista_de_parametros, 1), sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io, nombre_archivo_w, strlen(nombre_archivo_w) + 1);
                agregar_a_paquete(paquete_para_io, df_w, sizeof(df_w));
                agregar_a_paquete(paquete_para_io, tamanio_w, sizeof(tamanio_w));
                agregar_a_paquete(paquete_para_io, puntero_archivo_w, sizeof(puntero_archivo_w));
                free(nombre_archivo_w);
                break;
            }

            enviar_paquete(paquete_para_io, tes->cliente, logger);

            // espero la respuesta de IO
            recibir_operacion(tes->cliente, logger);
            t_list *resp = recibir_paquete(tes->cliente, logger);
            uint8_t resultado = (uint8_t)list_get(resp, 0);
            if (resultado)
            {
                log_info(logger, "La interfaz %s ejecuto correctamente la instruccion.", tes->nombre_interfaz);
                evaluar_BLOCKED_a_READY(tmb);
                // para el caso donde solamente hay 1 proceso, hago RaE
                evaluar_READY_a_EXEC();
            }
            else
            {
                log_error(logger, "La interfaz %s ejecuto erroneamente la instruccion.", tes->nombre_interfaz);
                // consego el pcb
                evaluar_BLOCKED_a_EXIT(devolver_pcb_desde_lista(lista_de_pcbs, pid_con_datos->pid));
            }
            list_destroy(resp);
            list_destroy(lista_de_parametros);
        }
    }
    free(nombreInterfaz);
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
    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INTERRUPTED_BY_USER >", id); // log obligatorio
    t_PCB *pcb_a_finalizar = devolver_pcb_desde_lista(lista_de_pcbs, id);

    // si yo tengo un recurso asignado y me van a matar lo tengo que liberar, por lo tanto valido si algun proceso puede ser desbloqueado, si es el caso tengo que hacer blocked a ready

    if (pcb_a_finalizar == NULL)
    {
        log_error(logger, "El proceso %d no existe.");
        return;
    }
    e_estado_proceso estado = pcb_a_finalizar->estado;
    // hay que hacer lo de buscar el recurso que tengo disponible

    sacarle_sus_recursos(pcb_a_finalizar->processID); // busca en los recursos donde esta el proceso y lo aumento y le aviso por si alguno se puede desbloquear

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
    if (queue_is_empty(cola))
        return false;
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

bool eliminar_id_de_la_cola_blocked(t_queue *cola, uint32_t id)
{
    if (queue_is_empty(cola))
        return false;
    bool loEncontre = false;
    t_pid_con_datos *primerId = queue_pop(cola);
    if (primerId->pid != id)
    {
        queue_push(cola, primerId);
        while (queue_peek(cola) != primerId) // ya di toda la vuelta
        {
            t_pid_con_datos *idActual = queue_pop(cola);
            if (idActual->pid == id)
                loEncontre = true; // es el id q tengo q sacar
            else
                queue_push(cola, idActual);
        }
    }
    else
    {
        loEncontre = true;
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
        pthread_mutex_lock(&mutex_grado_multiprogramacion);
        if (cant_procesos_ejecutando < grado_multiprogramacion)
        {
            pthread_mutex_unlock(&mutex_grado_multiprogramacion);
            uint32_t id = queue_pop(cola_NEW);

            pthread_mutex_lock(&mutex_cola_ready);
            queue_push(cola_READY, id);
            pthread_mutex_unlock(&mutex_cola_ready);
            t_PCB *pcb_elegido = obtener_pcb_de_lista_por_id(id);
            pcb_elegido->estado = E_READY;

            log_info(logger, "PID: < %u > - Estado Anterior: < NEW > - Estado Actual: < READY >", id); // log obligatorio
            log_obligatorio_ready(cola_READY, 0);
            cant_procesos_ejecutando++;
            log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
        }
        else
        {
            pthread_mutex_unlock(&mutex_grado_multiprogramacion);
            log_trace(logger, "No puedo porque el grado de multiprogramacion (%d) no admite mas programas en el sistema (actual: %d).", grado_multiprogramacion, cant_procesos_ejecutando);
            break;
        }
    }
}

void evaluar_READY_a_EXEC() // hilar (me olvide xq xD), (me acorde y no lo voy a hacer)
{
    log_trace(logger, "Voy a evaluar si puedo mover un proceso de READY a EXEC (asignar un proceso al CPU).");
    pthread_mutex_lock(&mutex_cola_ready);
    if (queue_is_empty(cola_RUNNING) && (!queue_is_empty(cola_READY) || !queue_is_empty(cola_READY_PRIORITARIA)) && !esta_planificacion_pausada) // valido q no este nadie corriendo, ready no este vacio y la planificacion no este pausada
    {
        pthread_mutex_unlock(&mutex_cola_ready); // tengo q hacer algo distinto segun cada algoritmo de planificacion
        uint32_t id;

        switch (algoritmo_planificacion)
        {
        case FIFO: // saco el primero
            log_trace(logger, "Entre a planificacion por FIFO.");
            pthread_mutex_lock(&mutex_cola_ready);
            id = queue_pop(cola_READY);
            pthread_mutex_unlock(&mutex_cola_ready);
            break;
        case RR: // igual q fifo pero creo el hilo con el quantum
            log_trace(logger, "Entre a planificacion por RR.");
            pthread_mutex_lock(&mutex_cola_ready);
            id = queue_pop(cola_READY);
            pthread_mutex_unlock(&mutex_cola_ready);
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
                pthread_mutex_lock(&mutex_cola_ready);
                id = queue_pop(cola_READY);
                pthread_mutex_unlock(&mutex_cola_ready);
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
        // log_trace(logger, "El proceso %d va a ser asignado al CPU.", id);
        log_info(logger, "PID: < %u > - Estado Anterior: < READY > - Estado Actual: < EXEC >", id); // log obligatorio
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
        pthread_mutex_unlock(&mutex_cola_ready);
        log_trace(logger, "No fue posible asignar un proceso al CPU.");
        log_debug(logger, "Cant. procesos en READY: %d | Cant. procesos en RUNNING: %d", queue_size(cola_READY), queue_size(cola_RUNNING));
    }
}

void evaluar_NEW_a_EXIT(t_PCB *pcb)
{
    log_info(logger, "PID: < %u > - Estado Anterior: < NEW > - Estado Actual: < EXIT >", pcb->processID); // log obligatorio
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
        cosas_vrr_cuando_se_desaloja_un_proceso(pcb); // ya hago aca lo de cambiar el quantum
        // hago las cosas especificas de VRR
        if (debe_ir_a_cola_prioritaria(pcb)) // se fija si estoy en vrr y si tiene q ir a prio
        {
            pcb->estado = E_READY_PRIORITARIO;
            queue_push(cola_READY_PRIORITARIA, pcb->processID);
            log_info(logger, "PID: < %u > - Estado Anterior: < EXEC > - Estado Actual: < READY + >", pcb->processID); // log obligatorio
        }
        else
        {
            pcb->estado = E_READY;
            pthread_mutex_lock(&mutex_cola_ready);
            queue_push(cola_READY, pcb->processID);
            pthread_mutex_unlock(&mutex_cola_ready);

            log_info(logger, "PID: < %u > - Estado Anterior: < EXEC > - Estado Actual: < READY >", pcb->processID); // log obligatorio
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
    log_info(logger, "PID: < %u > - Estado Anterior: < READY > - Estado Actual: < EXIT >", pcb->processID); // log obligatorio
    // libero la memoria
    liberar_memoria(pcb->processID);
    // le cambio el estado
    pcb->estado = E_EXIT;
    // lo popeo de su cola actual
    pthread_mutex_lock(&mutex_cola_ready);
    eliminar_id_de_la_cola(cola_READY, pcb->processID);
    pthread_mutex_unlock(&mutex_cola_ready);
    // lo pusheo en exit
    queue_push(cola_EXIT, pcb->processID);
    // desminuyo el contador de procesos
    cant_procesos_ejecutando--;
    log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
}

void evaluar_BLOCKED_a_EXIT(t_PCB *pcb)
{
    t_manejo_bloqueados *tmb = conseguir_tmb(pcb->processID); // si me interesa tener esta tmb y la funcion me permite eliminar a este de blocked
    // Tambien se recupera el recurso del proceso bloqueado que se pidio antes :p
    if (tmb->identificador == RECURSO)
    {
        t_manejo_recursos *tmr = (t_manejo_recursos *)tmb->datos_bloqueados;
        tmr->instancias_recursos++;
    }
    log_info(logger, "PID: < %u > - Estado Anterior: < BLOCKED > - Estado Actual: < EXIT >", pcb->processID); // log obligatorio
    // libero la memoria
    liberar_memoria(pcb->processID);
    // le cambio el estado
    pcb->estado = E_EXIT;
    // lo pusheo en exit
    queue_push(cola_EXIT, pcb->processID);
    // desminuyo el contador de procesos
    cant_procesos_ejecutando--;
    log_debug(logger, "Grado de multiprogramacion actual: %d", cant_procesos_ejecutando);
}

t_manejo_bloqueados *conseguir_tmb(uint32_t id)
{
    for (uint8_t i = 0; i < dictionary_size(diccionario_recursos_e_interfaces); i++) // podria ser un poquito mas lindo esto pero bueno, andar anda
    {
        t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos_e_interfaces, list_get(dictionary_keys(diccionario_recursos_e_interfaces), i));
        int size_queue = queue_size(tmb->cola_bloqueados);
        switch (tmb->identificador)
        {
        case RECURSO:
            if (eliminar_id_de_la_cola_blocked(tmb->cola_bloqueados, id))
            {
                return tmb;
            }
            break;
        case INTERFAZ:
            t_entrada_salida *tes = (t_entrada_salida *)tmb->datos_bloqueados;
            wait_interfaz(tes);
            if (eliminar_id_de_la_cola_blocked(tmb->cola_bloqueados, id))
            {
                return tmb;
            }
            signal_interfaz(tes);
            break;
        }
    }
    return NULL;
}

void evaluar_EXEC_a_BLOCKED(char *key, t_list *lista) // antes era recurso, ahora puede ser tanto recurso como nombre interfaz
{
    log_trace(logger, "Voy a evaluar si puedo mover un proceso de la cola EXEC a BLOCKED.");
    if (!queue_is_empty(cola_RUNNING))
    {
        t_PCB *pcb = devolver_pcb_desde_lista(lista_de_pcbs, (uint32_t)queue_pop(cola_RUNNING));
        pcb->estado = E_BLOCKED;
        cosas_vrr_cuando_se_desaloja_un_proceso(pcb); // ya hago aca lo de cambiar el quantum
        // usando la key, consigo el t_manejo_bloqueados y meto el id en la cola
        t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos_e_interfaces, key); // no valido q el recurso o interfaz exita, porque ya se valido antes
        t_pid_con_datos *pid_con_datos;
        switch (tmb->identificador)
        {
        case RECURSO:
            pid_con_datos = malloc(sizeof(t_pid_con_datos));
            pid_con_datos->pid = pcb->processID;
            log_trace(logger, "proceso bloqueado %u", pid_con_datos->pid);
            queue_push(tmb->cola_bloqueados, pid_con_datos);
            break;
        case INTERFAZ:
            t_entrada_salida *tes = (t_entrada_salida *)tmb->datos_bloqueados;
            pid_con_datos = malloc(sizeof(t_pid_con_datos));
            pid_con_datos->pid = pcb->processID;
            pid_con_datos->tipo_parametros_io = (e_tipo_interfaz)list_get(lista, 0);
            list_remove(lista, 0);
            pid_con_datos->datos = lista;

            log_info(logger, "PID: < %u > - Bloqueado por: < %s >", pcb->processID, tes->nombre_interfaz); // log obligatorio

            wait_interfaz(tes);
            queue_push(tmb->cola_bloqueados, pid_con_datos);
            signal_interfaz(tes);
            // esto es para desbloquear el hilo que atiende el envio a IO
            pthread_mutex_unlock(&(tes->binario));
            log_trace(logger, "Se desbloqueo el binario de la interfaz %s", tes->nombre_interfaz);
            break;
        }

        log_info(logger, "PID: < %u > - Estado Anterior: < EXEC > - Estado Actual: < BLOCKED >", pcb->processID); // log obligatorio
    }
    else
    {
        log_trace(logger, "No fue posible mover un proceso de EXEC a BLOCKED, por que no habia.");
    }
    evaluar_READY_a_EXEC(); // planifico xq se libero la cpu
}

void evaluar_BLOCKED_a_READY(t_manejo_bloqueados *tmb)
{ // desbloqueo por fifo
    log_trace(logger, "Voy a evaluar si puedo mover a algun proceso de la cola BLOCKED a READY.");
    t_queue *colaRecurso = tmb->cola_bloqueados;
    t_pid_con_datos *pid_con_datos;
    switch (tmb->identificador)
    {
    case RECURSO:
        if (queue_is_empty(colaRecurso))
        { // no hay nadie q desbloquear
            log_trace(logger, "No hay procesos bloqueados por el recurso.");
            return;
        }
        else if (((t_manejo_recursos *)tmb->datos_bloqueados)->instancias_recursos < 0)
        {
            log_trace(logger, "No voy a desbloquear a nadie porque el valor del semaforo es %u.", ((t_manejo_recursos *)tmb->datos_bloqueados)->instancias_recursos);
            return;
        }
        else
        {
            pid_con_datos = queue_pop(colaRecurso);
            log_trace(logger, "Proceso a desbloquear: %u.", pid_con_datos->pid);
        }
        break;
    case INTERFAZ:
        t_entrada_salida *tes = (t_entrada_salida *)tmb->datos_bloqueados;
        wait_interfaz(tes);
        if (queue_is_empty(colaRecurso))
        { // no hay nadie q desbloquear
            signal_interfaz(tes);
            log_trace(logger, "No hay procesos bloqueados por el recurso.");
            return;
        }
        pid_con_datos = queue_pop(colaRecurso);
        signal_interfaz(tes);
        break;
    }

    t_PCB *pcb = devolver_pcb_desde_lista(lista_de_pcbs, pid_con_datos->pid);
    free(pid_con_datos);
    // hago las cosas especificas de VRR
    if (debe_ir_a_cola_prioritaria(pcb)) // se fija si estoy en vrr y si tiene q ir a prio
    {
        log_info(logger, "PID: < %u > - Estado Anterior: < BLOCKED > - Estado Actual: < READY + >", pcb->processID); // log obligatorio
        pcb->estado = E_READY_PRIORITARIO;
        queue_push(cola_READY_PRIORITARIA, pcb->processID);
        log_obligatorio_ready(cola_READY_PRIORITARIA, 1);
    }
    else
    {                                                                                                              // entra aca si estoy en FIFO y RR
        log_info(logger, "PID: < %u > - Estado Anterior: < BLOCKED > - Estado Actual: < READY >", pcb->processID); // log obligatorio
        pcb->estado = E_READY;
        pthread_mutex_lock(&mutex_cola_ready);
        queue_push(cola_READY, pcb->processID);
        pthread_mutex_unlock(&mutex_cola_ready);
        log_obligatorio_ready(cola_READY, 0);
    }
}

void evaluar_EXEC_a_EXIT()
{
    uint32_t id = (uint32_t)queue_pop(cola_RUNNING);
    log_info(logger, "PID: < %u > - Estado Anterior: < EXEC > - Estado Actual: < EXIT >", id); // log obligatorio

    liberar_memoria(id);

    t_PCB *pcb_en_running = devolver_pcb_desde_lista(lista_de_pcbs, id);
    cosas_vrr_cuando_se_desaloja_un_proceso(pcb_en_running); // ya hago aca lo de cambiar el quantum
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
        log_info(logger, "\n\n|################################|\n"
                         "|         MENU DE OPCIONES       |\n"
                         "|################################|\n"
                         "|  INICIAR_PROCESO [IP] [PATH]   |\n"
                         "|     PROCESO_ESTADO [PE]        |\n"
                         "|  FINALIZAR_PROCESO [FP]        |\n"
                         "|  INICIAR_PLANIFICACION [IPL]   |\n"
                         "|  DETENER_PLANIFICACION [DP]    |\n"
                         "|  EJECUTAR_SCRIPT [ES] [PATH]   |\n"
                         "| MULTIPROGRAMACION [MP] [VALOR] |\n"
                         "|          SALIR                 |\n"
                         "|################################|\n");
    else
        printf("\n\n|################################|\n"
               "|         MENU DE OPCIONES       |\n"
               "|################################|\n"
               "|  INICIAR_PROCESO [IP] [PATH]   |\n"
               "|     PROCESO_ESTADO [PE]        |\n"
               "|  FINALIZAR_PROCESO [FP]        |\n"
               "|  INICIAR_PLANIFICACION [IPL]   |\n"
               "|  DETENER_PLANIFICACION [DP]    |\n"
               "|  EJECUTAR_SCRIPT [ES] [PATH]   |\n"
               "| MULTIPROGRAMACION [MP] [VALOR] |\n"
               "|          SALIR                 |\n"
               "|################################|\n");
}

void *atender_respuesta_proceso(void *arg)
{

    uint32_t idProcesoActual = queue_peek(cola_RUNNING); // esto es para el log del final
    log_trace(logger, "Entre a un hilo para esperar la finalizacion del proceso %d.", idProcesoActual);

    // aca tengo q pausar la planificacion si me metieron un DETENER_PLANIFICACION
    // await(esta_planificacion_pausada)
    t_PCB *pcb_en_running;
    bool sigo_esperando_cosas_de_cpu = true;
    while (sigo_esperando_cosas_de_cpu)
    {
        int op = recibir_operacion(cliente_cpu_dispatch, logger); // si, uso el cliente como socket servidor

        if (op == PAQUETE)
        {
            t_list *lista_respuesta_cpu = recibir_paquete(cliente_cpu_dispatch, logger);
            log_trace(logger, "CPU me devolvio el contexto de ejecucion.");
            pcb_en_running = devolver_pcb_desde_lista(lista_de_pcbs, idProcesoActual);
            actualizar_pcb(lista_respuesta_cpu, pcb_en_running, logger);
            // ---------------------------------------------- //
            e_motivo_desalojo motivo_desalojo = conseguir_motivo_desalojo_de_registros_empaquetados(lista_respuesta_cpu);
            char *motivo_desalojo_en_texto = motivo_desalojo_texto(motivo_desalojo);
            log_trace(logger, "Motivo de desalojo de %d: %s", pcb_en_running->processID, motivo_desalojo_en_texto);
            free(motivo_desalojo_en_texto);

            switch (motivo_desalojo)
            {
            case MOTIVO_DESALOJO_OUT_OF_MEMORY:
                // log_error(logger, "El proceso %d se quedo sin memoria.", pcb_en_running->processID);
                sacarle_sus_recursos(pcb_en_running->processID);
                log_info(logger, "Finaliza el proceso < %u > - Motivo: < OUT_OF_MEMORY >", pcb_en_running->processID); // log obligatorio
                evaluar_EXEC_a_EXIT();
                // termino el ciclo
                sigo_esperando_cosas_de_cpu = false;
                break;
            case MOTIVO_DESALOJO_EXIT:
                log_info(logger, "Finaliza el proceso < %u > - Motivo: < SUCCESS >", pcb_en_running->processID); // log obligatorio
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
                    enviar_paquete(respuesta_para_cpu, cliente_cpu_dispatch, logger);
                }
                else if (resultado_asignar_recurso == 1)
                { // 1: no te la di (te bloqueo)
                    log_trace(logger, "Voy a enviarle al CPU que no tiene la instancia, asi q sera bloqueado.");
                    log_info(logger, "PID: < %u > - Bloqueado por: < %s >", pcb_en_running->processID, argWait); // log obligatorio
                    agregar_a_paquete(respuesta_para_cpu, 1, sizeof(uint8_t));
                    enviar_paquete(respuesta_para_cpu, cliente_cpu_dispatch, logger);
                    evaluar_EXEC_a_BLOCKED(argWait, NULL);
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                else if (resultado_asignar_recurso == 2)
                { // 2: mato al proceso xq pidio algo nada q ver
                    // log_error(logger, "El cpu me pidio un recurso que no existe. Lo tenemos que matar!");
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_RESOURCE >", pcb_en_running->processID); // log obligatorio
                    agregar_a_paquete(respuesta_para_cpu, 1, sizeof(uint8_t));                                                // le mando un 1 xq para cpu es lo mismo matar el proceso que bloquearlo
                    enviar_paquete(respuesta_para_cpu, cliente_cpu_dispatch, logger);
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(argWait);
                break;
            case MOTIVO_DESALOJO_SIGNAL:
                char *nombreRecurso = list_get(lista_respuesta_cpu, 13); // hacer desp esto en una funcion
                log_debug(logger, "Argumento del signal: %s", nombreRecurso);
                t_paquete *respuesta_para_cpu_signal = crear_paquete();
                // desasigno
                uint8_t resultado_asignar_recurso_signal = desasignar_recurso(nombreRecurso, pcb_en_running);
                if (resultado_asignar_recurso_signal == 0)
                { // hay instancias disponibles, voy a desbloquear a alguien y le respondo a cpu
                    agregar_a_paquete(respuesta_para_cpu_signal, resultado_asignar_recurso_signal, sizeof(uint8_t));
                    enviar_paquete(respuesta_para_cpu_signal, cliente_cpu_dispatch, logger);
                    evaluar_BLOCKED_a_READY((t_manejo_bloqueados *)dictionary_get(diccionario_recursos_e_interfaces, nombreRecurso));
                    log_trace(logger, "Voy a enviarle al CPU que salio todo bien.");
                }
                else if (respuesta_para_cpu_signal == 1)
                { // le digo a cpu q desaloje el proceso y lo mando a exit
                    // log_error(logger, "El cpu me pidio un recurso que no existe. Lo tenemos que matar!");
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_RESOURCE >", pcb_en_running->processID); // log obligatorio
                    agregar_a_paquete(respuesta_para_cpu_signal, resultado_asignar_recurso_signal, sizeof(uint8_t));
                    enviar_paquete(respuesta_para_cpu_signal, cliente_cpu_dispatch, logger);
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombreRecurso);
                break;
            case MOTIVO_DESALOJO_INTERRUPCION:
                // termino el ciclo
                sigo_esperando_cosas_de_cpu = false;
                // planifico
                evaluar_EXEC_a_READY();
                break;
            case MOTIVO_DESALOJO_IO_GEN_SLEEP:
                char *nombre_interfaz = list_get(lista_respuesta_cpu, 13);
                uint32_t cant = list_get(lista_respuesta_cpu, 14);
                log_debug(logger, "Argumentos del IO_GEN_SLEEP: %s | %u", nombre_interfaz, cant);

                if (verificar_interfaz(nombre_interfaz))
                {
                    // t_entrada_salida *tes = obtener_entrada_salida(nombre_interfaz);
                    t_manejo_bloqueados *tmb_sleep = dictionary_get(diccionario_recursos_e_interfaces, nombre_interfaz);
                    t_entrada_salida *tes_sleep = (t_entrada_salida *)tmb_sleep->datos_bloqueados;

                    // armo los datos
                    t_list *l_io_sleep = list_create();
                    list_add(l_io_sleep, GENERICA);
                    list_add(l_io_sleep, cant);
                    list_add(l_io_sleep, pcb_en_running->processID);

                    // estos wait y signal NO van xq adentro del la funcion hago otros, por lo tanto termino en una especie de deadlock xq hago wait 2 veces y nunca llego al signal
                    // wait_interfaz(tes_sleep);
                    evaluar_EXEC_a_BLOCKED(nombre_interfaz, l_io_sleep);
                    // signal_interfaz(tes_sleep);

                    sigo_esperando_cosas_de_cpu = false;
                }
                else
                {
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_INTERFACE >", pcb_en_running->processID); // log obligatorio
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombre_interfaz);
                break;
            case MOTIVO_DESALOJO_IO_STDIN_READ:
                char *nombre_interfaz_stdin = list_get(lista_respuesta_cpu, 13);
                uint32_t df_stdin = list_get(lista_respuesta_cpu, 14);
                uint32_t tamanio_stdin = list_get(lista_respuesta_cpu, 15);
                log_debug(logger, "Argumentos del IO_STDIN_READ: %s | %u | %u", nombre_interfaz_stdin, df_stdin, tamanio_stdin);

                if (verificar_interfaz(nombre_interfaz_stdin))
                {
                    t_manejo_bloqueados *tmb_stdin = dictionary_get(diccionario_recursos_e_interfaces, nombre_interfaz_stdin);
                    t_entrada_salida *tes_stdin = (t_entrada_salida *)tmb_stdin->datos_bloqueados;
                    // armo los datos
                    t_list *l_io_stdin_read = list_create();
                    list_add(l_io_stdin_read, STDIN);
                    list_add(l_io_stdin_read, df_stdin);
                    list_add(l_io_stdin_read, tamanio_stdin);
                    list_add(l_io_stdin_read, pcb_en_running->processID);

                    evaluar_EXEC_a_BLOCKED(nombre_interfaz_stdin, l_io_stdin_read);
                    sigo_esperando_cosas_de_cpu = false;
                }
                else
                {
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_INTERFACE >", pcb_en_running->processID); // log obligatorio
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombre_interfaz_stdin);
                break;
            case MOTIVO_DESALOJO_IO_STDOUT_WRITE:
                char *nombre_interfaz_write = list_get(lista_respuesta_cpu, 13);
                uint32_t df_write = list_get(lista_respuesta_cpu, 14);
                uint32_t tamanio_write = list_get(lista_respuesta_cpu, 15);
                log_debug(logger, "Argumentos del IO_STDOUT_WRITE: %s | %u | %u", nombre_interfaz_write, df_write, tamanio_write);

                if (verificar_interfaz(nombre_interfaz_write))
                {
                    t_manejo_bloqueados *tmb_stdout = dictionary_get(diccionario_recursos_e_interfaces, nombre_interfaz_write);
                    t_entrada_salida *tes_stdout = (t_entrada_salida *)tmb_stdout->datos_bloqueados;

                    // armo los datos
                    t_list *l_io_stdout_write = list_create();
                    list_add(l_io_stdout_write, STDOUT);
                    list_add(l_io_stdout_write, df_write);
                    list_add(l_io_stdout_write, tamanio_write);
                    list_add(l_io_stdout_write, pcb_en_running->processID);

                    evaluar_EXEC_a_BLOCKED(nombre_interfaz_write, l_io_stdout_write);

                    sigo_esperando_cosas_de_cpu = false;
                }
                else
                {
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_INTERFACE >", pcb_en_running->processID); // log obligatorio
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombre_interfaz_write);
                break;
            case MOTIVO_DESALOJO_IO_FS_CREATE:
                char *nombre_interfaz_create = list_get(lista_respuesta_cpu, 13);
                char *nombre_archivo_create = list_get(lista_respuesta_cpu, 14);
                log_debug(logger, "Argumentos del IO_FS_CREATE: %s | %s", nombre_interfaz_create, nombre_archivo_create);
                if (verificar_interfaz(nombre_interfaz_create))
                {
                    t_manejo_bloqueados *tmb_dialfs_create = dictionary_get(diccionario_recursos_e_interfaces, nombre_interfaz_create);
                    t_entrada_salida *tes_dialfs_create = (t_entrada_salida *)tmb_dialfs_create->datos_bloqueados;

                    // armo los datos
                    t_list *l_io_fs_create = list_create();
                    list_add(l_io_fs_create, DIALFS_CREATE);
                    list_add(l_io_fs_create, string_duplicate(nombre_archivo_create));
                    list_add(l_io_fs_create, pcb_en_running->processID);

                    evaluar_EXEC_a_BLOCKED(nombre_interfaz_create, l_io_fs_create);

                    sigo_esperando_cosas_de_cpu = false;
                }
                else
                {
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_INTERFACE >", pcb_en_running->processID); // log obligatorio
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombre_archivo_create);
                free(nombre_interfaz_create);
                break;
            case MOTIVO_DESALOJO_IO_FS_DELETE:
                char *nombre_interfaz_delete = list_get(lista_respuesta_cpu, 13);
                char *nombre_archivo_delete = list_get(lista_respuesta_cpu, 14);
                log_debug(logger, "Argumentos del IO_FS_DELETE: %s | %s", nombre_interfaz_delete, nombre_archivo_delete);
                if (verificar_interfaz(nombre_interfaz_delete))
                {
                    t_manejo_bloqueados *tmb_dialfs_delete = dictionary_get(diccionario_recursos_e_interfaces, nombre_interfaz_delete);
                    t_entrada_salida *tes_dialfs_delete = (t_entrada_salida *)tmb_dialfs_delete->datos_bloqueados;

                    // armo los datos
                    t_list *l_io_fs_delete = list_create();
                    list_add(l_io_fs_delete, DIALFS_DELETE);
                    list_add(l_io_fs_delete, string_duplicate(nombre_archivo_delete));
                    list_add(l_io_fs_delete, pcb_en_running->processID);

                    evaluar_EXEC_a_BLOCKED(nombre_interfaz_delete, l_io_fs_delete);

                    sigo_esperando_cosas_de_cpu = false;
                }
                else
                {
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_INTERFACE >", pcb_en_running->processID); // log obligatorio
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombre_archivo_delete);
                free(nombre_interfaz_delete);
                break;
            case MOTIVO_DESALOJO_IO_FS_TRUNCATE:
                char *nombre_interfaz_truncate = list_get(lista_respuesta_cpu, 13);
                char *nombre_archivo_truncate = list_get(lista_respuesta_cpu, 14);
                uint32_t tamanio_truncate = list_get(lista_respuesta_cpu, 15);
                log_debug(logger, "Argumentos del IO_FS_TRUNCATE: %s | %s | %u", nombre_interfaz_truncate, nombre_archivo_truncate, tamanio_truncate);
                if (verificar_interfaz(nombre_interfaz_truncate))
                {
                    t_manejo_bloqueados *tmb_dialfs_truncate = dictionary_get(diccionario_recursos_e_interfaces, nombre_interfaz_truncate);
                    t_entrada_salida *tes_dialfs_truncate = (t_entrada_salida *)tmb_dialfs_truncate->datos_bloqueados;

                    // armo los datos
                    t_list *l_io_fs_truncate = list_create();
                    list_add(l_io_fs_truncate, DIALFS_TRUNCATE);
                    list_add(l_io_fs_truncate, string_duplicate(nombre_archivo_truncate));
                    list_add(l_io_fs_truncate, pcb_en_running->processID);
                    list_add(l_io_fs_truncate, tamanio_truncate);

                    evaluar_EXEC_a_BLOCKED(nombre_interfaz_truncate, l_io_fs_truncate);

                    sigo_esperando_cosas_de_cpu = false;
                }
                else
                {
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_INTERFACE >", pcb_en_running->processID); // log obligatorio
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombre_archivo_truncate);
                free(nombre_interfaz_truncate);
                break;
            case MOTIVO_DESALOJO_IO_FS_WRITE:
                char *nombre_interfaz_fs_write = list_get(lista_respuesta_cpu, 13);
                char *nombre_archivo_write = list_get(lista_respuesta_cpu, 14);
                uint32_t df_fs_write = list_get(lista_respuesta_cpu, 15);
                uint32_t tamanio_fs_write = list_get(lista_respuesta_cpu, 16);
                uint32_t punterio_archivo_write = list_get(lista_respuesta_cpu, 17);
                log_debug(logger, "Argumentos del IO_FS_WRITE: %s | %s | %u | %u | %u", nombre_interfaz_fs_write, nombre_archivo_write, df_fs_write, tamanio_fs_write, punterio_archivo_write);
                if (verificar_interfaz(nombre_interfaz_fs_write))
                {
                    t_manejo_bloqueados *tmb_dialfs_write = dictionary_get(diccionario_recursos_e_interfaces, nombre_interfaz_fs_write);
                    t_entrada_salida *tes_dialfs_write = (t_entrada_salida *)tmb_dialfs_write->datos_bloqueados;

                    // armo los datos
                    t_list *l_io_fs_write = list_create();
                    list_add(l_io_fs_write, DIALFS_WRITE);
                    list_add(l_io_fs_write, string_duplicate(nombre_archivo_write));
                    list_add(l_io_fs_write, pcb_en_running->processID);
                    list_add(l_io_fs_write, df_fs_write);
                    list_add(l_io_fs_write, tamanio_fs_write);
                    list_add(l_io_fs_write, punterio_archivo_write);
                    evaluar_EXEC_a_BLOCKED(nombre_interfaz_fs_write, l_io_fs_write);

                    sigo_esperando_cosas_de_cpu = false;
                }
                else
                {
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_INTERFACE >", pcb_en_running->processID); // log obligatorio
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombre_archivo_write);
                free(nombre_interfaz_fs_write);
                break;
            case MOTIVO_DESALOJO_IO_FS_READ:
                char *nombre_interfaz_read = list_get(lista_respuesta_cpu, 13);
                char *nombre_archivo_read = list_get(lista_respuesta_cpu, 14);
                uint32_t df_read = list_get(lista_respuesta_cpu, 15);
                uint32_t tamanio_read = list_get(lista_respuesta_cpu, 16);
                uint32_t punterio_archivo_read = list_get(lista_respuesta_cpu, 17);
                log_debug(logger, "Argumentos del IO_FS_READ: %s | %s | %u | %u | %u", nombre_interfaz_read, nombre_archivo_read, df_read, tamanio_read, punterio_archivo_read);
                if (verificar_interfaz(nombre_interfaz_read))
                {
                    t_manejo_bloqueados *tmb_dialfs_read = dictionary_get(diccionario_recursos_e_interfaces, nombre_interfaz_read);
                    t_entrada_salida *tes_dialfs_read = (t_entrada_salida *)tmb_dialfs_read->datos_bloqueados;

                    // armo los datos
                    t_list *l_io_fs_read = list_create();
                    list_add(l_io_fs_read, DIALFS_READ);
                    list_add(l_io_fs_read, string_duplicate(nombre_archivo_read));
                    list_add(l_io_fs_read, pcb_en_running->processID);
                    list_add(l_io_fs_read, df_read);
                    list_add(l_io_fs_read, tamanio_read);
                    list_add(l_io_fs_read, punterio_archivo_read);

                    evaluar_EXEC_a_BLOCKED(nombre_interfaz_read, l_io_fs_read);

                    sigo_esperando_cosas_de_cpu = false;
                }
                else
                {
                    log_info(logger, "Finaliza el proceso < %u > - Motivo: < INVALID_INTERFACE >", pcb_en_running->processID); // log obligatorio
                    evaluar_EXEC_a_EXIT();
                    // termino el ciclo
                    sigo_esperando_cosas_de_cpu = false;
                }
                free(nombre_archivo_read);
                free(nombre_interfaz_read);
                break;
            default:
                log_error(logger, "Recibi cualquier cosa como motivo de desalojo");
                break;
            }
            list_destroy(lista_respuesta_cpu);
        }
        else
        {
            log_error(logger, "Me mandaron cualquier cosa, voy a romper todo.");
            // termino el ciclo
            sigo_esperando_cosas_de_cpu = false;
        }
    }
    // lo hago aca al final xq es cuando se q el proceso fue desalojado
    log_trace(logger, "Termino el hilo para el proceso %d.", idProcesoActual);
}

void obtener_valores_de_recursos()
{
    diccionario_recursos_e_interfaces = dictionary_create();
    char **lista1 = config_get_array_value(config, "RECURSOS");
    char **lista2 = config_get_array_value(config, "INSTANCIAS_RECURSOS");

    int i = 0;
    while (true)
    {
        if ((lista1)[i] == NULL)
            break;
        t_manejo_bloqueados *tmb = crear_manejo_bloqueados(RECURSO);
        ((t_manejo_recursos *)(tmb->datos_bloqueados))->instancias_recursos = atoi(lista2[i]);
        ((t_manejo_recursos *)(tmb->datos_bloqueados))->lista_procesos_usando_recurso = list_create();
        dictionary_put(diccionario_recursos_e_interfaces, (lista1)[i], tmb);
        i++;
    }
}

uint8_t asignar_recurso(char *recurso, t_PCB *pcb)
{
    t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos_e_interfaces, recurso);
    uint8_t r;
    if (tmb != NULL)
    { // existe, esta todo piola
        // hasta aca llegue (los logs de abajo no los hace)
        t_manejo_recursos *manejo_recurso = (t_manejo_recursos *)tmb->datos_bloqueados;
        log_debug(logger, "Valor del recurso %s antes de modificarlo: %d", recurso, manejo_recurso->instancias_recursos);

        manejo_recurso->instancias_recursos -= 1;
        log_debug(logger, "Valor del recurso %s desp de modifiarlo: %d", recurso, manejo_recurso->instancias_recursos);

        if (manejo_recurso->instancias_recursos < 0)
        {
            r = 1; // hay q bloquear el proceso
        }
        else
        {
            list_add(manejo_recurso->lista_procesos_usando_recurso, pcb->processID);
            r = 0; // lo devuelvo sin bloquear
        }
    }
    else
    { // cagaste
        r = 2;
    }
    return r;
}

uint8_t desasignar_recurso(char *recurso, t_PCB *pcb)
{
    t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos_e_interfaces, recurso);
    uint8_t r;
    if (tmb != NULL)
    {
        t_manejo_recursos *manejo_recurso = (t_manejo_recursos *)tmb->datos_bloqueados;
        log_debug(logger, "Valor del recurso %s antes de modificarlo: %d", recurso, manejo_recurso->instancias_recursos);
        list_remove_element(manejo_recurso->lista_procesos_usando_recurso, pcb->processID);
        manejo_recurso->instancias_recursos += 1;
        log_debug(logger, "Valor del recurso %s desp de modifiarlo: %d", recurso, manejo_recurso->instancias_recursos);
        r = 0;
    }
    else
    { // cagaste
        r = 1;
    }
    return r;
}

t_manejo_bloqueados *crear_manejo_bloqueados(e_tipo_bloqueado identificador)
{
    t_manejo_bloqueados *tmb = malloc(sizeof(t_manejo_bloqueados));
    tmb->identificador = identificador;
    tmb->cola_bloqueados = queue_create();
    switch (identificador)
    {
    case RECURSO:
        tmb->datos_bloqueados = malloc(sizeof(t_manejo_recursos));
        break;

    case INTERFAZ:
        tmb->datos_bloqueados = malloc(sizeof(t_entrada_salida));
        break;
    }
    return tmb;
}

void destruir_manejo_bloqueados(t_manejo_bloqueados *tmb)
{
    queue_destroy(tmb->cola_bloqueados);
    free(tmb);
}

// manejo de memoria

void liberar_memoria(uint32_t id)
{
    log_trace(logger, "Voy a decirle a memoria q libere lo del proceso %d", id);

    t_paquete *p = crear_paquete();
    agregar_a_paquete(p, BORRAR_PROCESO, sizeof(uint8_t)); // 1 es de borrar proceso
    agregar_a_paquete(p, id, sizeof(id));
    enviar_paquete(p, cliente_memoria, logger);

    recibir_operacion(cliente_memoria, logger);
    t_list *resp_memoria = recibir_paquete(cliente_memoria, logger);
    if (!list_get(resp_memoria, 0))
        log_debug(logger, "Memoria pudo liberar el proceso %u correctamente.", id);

    list_destroy(resp_memoria);
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
    recibir_operacion(cliente_memoria, logger);

    t_list *l = recibir_paquete(cliente_memoria, logger);
    log_debug(logger, "Respuesta recibida de Memoria: %u", (uint8_t)list_get(l, 0));
    v = ((uint8_t)list_get(l, 0) == 0);
    list_destroy(l);

    // recibir cualquier otra cosa implica false, por que? porque si
    return v;
}

// desarrollo del quantum

void *trigger_interrupcion_quantum(void *args) // escuchar audio q me mande a wsp
{
    t_PCB *pcb = (t_PCB *)args;
    log_trace(logger, "Entre al hilo para enviar la interrupcion de fin de quantum del proceso %u.", pcb->processID);
    log_debug(logger, "Valor del quantum: %u", pcb->quantum); // el %u es para unsigned, x las dudas

    usleep(pcb->quantum * 1000); // seteo el q de ms a s
    // el objetivo de esto es saber si el proceso sigue ejecutando
    // imaginate q yo creo el proceso y, por lo tanto, esta funcion. Desp el proceso se desaloja por alguna razon sin terminar su quantum
    // entonces reviso si el proceso en cuestion sigue ejecutando y, si es asi, mando la interrupcion, el problema de esto es:
    // como carajo consigo el quantum q me falto ejecutar? opcion q se me ocurrio, HAY OTRO HILO QUE LLEVA EL TIEMPO EJECUTANDO (NOSECOMO) Y LO DEVUELVE (TAMPOCOSECOMO) Y DESP LOS RESTO

    log_trace(logger, "Termino el quantum del proceso %u.", pcb->processID);

    if (!queue_is_empty(cola_RUNNING) && queue_peek(cola_RUNNING) == pcb->processID) // lo primero es xq si es el unico proceso en el sistema, voy a tener un sf haciendo el peek
    {
        log_info(logger, "PID: < %u > - Desalojado por fin de Quantum", pcb->processID); // log obligatorio
        t_paquete *paquete_interrupcion = crear_paquete();
        agregar_a_paquete(paquete_interrupcion, pcb->processID, sizeof(pcb->processID)); // le mando el id del proceso xq no se q mandar
        enviar_paquete(paquete_interrupcion, cliente_cpu_interrupt, logger);
    }
    else
    {
        log_trace(logger, "El proceso %u fue desalojado del CPU antes de que terminara su queantum, no mando nada.", pcb->processID);
    }

    log_trace(logger, "Termino el hilo para el quantum del proceso %u.", pcb->processID);
}

void cosas_vrr_cuando_se_desaloja_un_proceso(t_PCB *pcb)
{ // si estoy en vrr, dentengo el cronometro y le resto al quantum lo ejecutado para quedarme con lo q le falta
    if (algoritmo_planificacion == VRR)
    {
        temporal_stop(pcb->tiempo_en_ejecucion); // esto en si no es necesario pero bueno, por las dudas lo detengo
        int64_t tiempo_ejecutado = temporal_gettime(pcb->tiempo_en_ejecucion);
        log_debug(logger, "Tiempo que se ejecuto el programa: %d | Quantum: %d", tiempo_ejecutado, pcb->quantum);

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

void crear_interfaz(char *nombre_interfaz, int cliente)
{
    t_manejo_bloqueados *tmb = crear_manejo_bloqueados(INTERFAZ);
    t_entrada_salida *tes = (t_entrada_salida *)tmb->datos_bloqueados;

    tes->nombre_interfaz = string_duplicate(nombre_interfaz);
    tes->cliente = cliente;
    pthread_mutex_init(&(tes->mutex), NULL);
    pthread_mutex_init(&(tes->binario), NULL);
    pthread_mutex_lock(&(tes->binario)); // esto es porque quiero q arranque bloqueado y se desbloquee cuando meto a alguien en la queue
    wait_interfaz(tes);
    dictionary_put(diccionario_recursos_e_interfaces, string_duplicate(nombre_interfaz), tmb);
    signal_interfaz(tes);
}

void wait_interfaz(t_entrada_salida *tes)
{
    pthread_mutex_lock(&(tes->mutex));
}

void signal_interfaz(t_entrada_salida *tes)
{
    pthread_mutex_unlock(&(tes->mutex));
}

void sacarle_sus_recursos(uint32_t pid)
{
    log_trace(logger, "Voy a sacarle los recursos al proceso %d.", pid);
    t_list *lista_keys = dictionary_keys(diccionario_recursos_e_interfaces);
    for (uint8_t i = 0; i < dictionary_size(diccionario_recursos_e_interfaces); i++) // podria ser un poquito mas lindo esto pero bueno, andar anda
    {

        t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos_e_interfaces, list_get(lista_keys, i));
        if (tmb->identificador == RECURSO)
        {
            t_manejo_recursos *manejo_recurso = (t_manejo_recursos *)tmb->datos_bloqueados;

            bool seElimino = eliminar_id_lista(manejo_recurso->lista_procesos_usando_recurso, pid);

            if (seElimino)
            {
                log_trace(logger, "Se le saco un recurso al proceso %d.", pid);
                manejo_recurso->instancias_recursos++;
                evaluar_BLOCKED_a_READY(tmb);
            }
        }
    }
    /*for(uint8_t i = 0; i < dictionary_size(diccionario_recursos_e_interfaces); i++)
    {
        free(list_remove(lista_keys, i));
    }*/
    list_destroy(lista_keys);
}

bool eliminar_id_lista(t_list *lista, uint32_t id)
{
    bool seElimino = false;

    for (uint32_t i = 0; i < list_size(lista); i++)
    {
        uint32_t id_lista = (uint32_t)list_get(lista, i);
        if (id_lista == id)
        {
            uint32_t elemento = (uint32_t)list_remove(lista, i);
            seElimino = true;
            break;
        }
    }
    return seElimino;
}

bool verificar_interfaz(char *nombre_interfaz)
{
    return dictionary_has_key(diccionario_recursos_e_interfaces, nombre_interfaz);
}

void log_obligatorio_ready(t_queue *cola, uint8_t tipo_cola)
{
    pthread_mutex_lock(&mutex_cola_ready);
    t_list *lista_de_cola = cola->elements;
    char *pids = string_new(); // Buffer para almacenar los PIDs
    for (int i = 0; i < list_size(lista_de_cola); i++)
    {
        uint32_t id = list_get(lista_de_cola, i);
        char *id_texto = string_itoa(id);
        string_append(&pids, id_texto);
        free(id_texto);
        string_append(&pids, ", ");
    }
    pthread_mutex_unlock(&mutex_cola_ready);
    pids[strlen(pids) - 2] = '\0'; // Elimino la ultima coma
    if (tipo_cola == 0)            // READY
        log_info(logger, "Cola Ready: %s", pids);
    else
        log_info(logger, "Cola Ready Prioridad: %s", pids);

    free(pids);
}

void finalizar_kernel()
{
    // finaliza todos los procesos pendientes
    for (int i = 0; i < list_size(lista_de_pcbs); i++)
    {
        t_PCB *p = list_get(lista_de_pcbs, i);
        char *estado_texto = estado_proceso_texto(p->estado);
        log_debug(logger, "Info proceso a borrar: \nID: %d PC: %d Q: %d Estado: %s AX: %d BX: %d CX: %d DX: %d EAX: %d EBX: %d ECX: %d EDX SI: %d DI: %d", p->processID, p->programCounter, p->quantum, estado_texto, p->AX, p->BX, p->CX, p->DX, p->EAX, p->EBX, p->ECX, p->EDX, p->SI, p->DI);
        free(estado_texto);
        if (p->estado != E_EXIT)
        {
            eliminar_proceso(p->processID);
        }
        temporal_destroy(p->tiempo_en_ejecucion);
    }
    list_destroy_and_destroy_elements(lista_de_pcbs, free);

    liberar_conexion(cliente_cpu_dispatch, logger);
    liberar_conexion(cliente_cpu_interrupt, logger);
    liberar_conexion(cliente_memoria, logger);
    destruir_logger(logger);
    destruir_config(config);
    liberar_diccionario();
    queue_destroy(cola_EXIT);
    queue_destroy(cola_READY);
    queue_destroy(cola_NEW);
    queue_destroy(cola_RUNNING);
    queue_destroy(cola_READY_PRIORITARIA);
}

void liberar_diccionario()
{
    t_list *lista_keys = dictionary_keys(diccionario_recursos_e_interfaces);
    for (uint8_t i = 0; i < dictionary_size(diccionario_recursos_e_interfaces); i++) // podria ser un poquito mas lindo esto pero bueno, andar anda
    {
        t_manejo_bloqueados *tmb = dictionary_get(diccionario_recursos_e_interfaces, list_get(lista_keys, i));
        if (tmb->identificador == RECURSO)
        {
            t_manejo_recursos *manejo_recurso = (t_manejo_recursos *)tmb->datos_bloqueados;
            list_destroy(manejo_recurso->lista_procesos_usando_recurso);
            free(manejo_recurso);
        }
        else if (tmb->identificador == INTERFAZ)
        {
            t_entrada_salida *tes = (t_entrada_salida *)tmb->datos_bloqueados;
            free(tes->nombre_interfaz);
            pthread_mutex_destroy(&(tes->mutex));
            pthread_mutex_destroy(&(tes->binario));
            free(tes);
        }
        destruir_manejo_bloqueados(tmb);
    }
    list_destroy(lista_keys);
    dictionary_destroy(diccionario_recursos_e_interfaces);
}