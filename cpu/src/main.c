#include "main.h"

// Servidor
int socket_servidor_dispatch;
int socket_servidor_interrupt;
int socket_cliente_dispatch;
int socket_cliente_interrupt;

// Clientes
int cliente_memoria;

t_log *logger;
t_config *config;

// tabla TLB
t_TLB *TLB;
int tlb_size = 0;
int ultima_fila_modificada = 0;
int instante_referencia = 0;
// hilos
pthread_t tid[3];

// tamanio de pagina, lo necesito para la MMU
int tam_pag;
// registros
t_registros *registros;

// bit para la interrupion
bool interrupcion;

// mutex para la variable interrupcion
pthread_mutex_t mutex_interrupcion = PTHREAD_MUTEX_INITIALIZER;
// int interrupcion_init;

// Linea de instruccion que llega de memoria
char *linea_de_instruccion;

// instruccion
e_instruccion instruccion;

// Linea de instruccion separada
char **linea_de_instruccion_separada;

// para saber si sigo ejecutando el proceso actual
bool proceso_actual_ejecutando;

// para saber si despues de ejecutar el proceso tengo que mandar el pcb o ya se hizo antes
bool mandar_pcb;

// para saber si ya se mando el pcb
bool ya_se_mando_pcb;

int main(int argc, char *argv[])
{
    tam_pag = conseguir_tam_pag();

    // MMU va a ser otro hilo aparte
    // Inicializo las variables
    interrupcion = false;
    // interrupcion_init = pthread_mutex_init(&mutex_interrupcion, NULL);
    pthread_mutex_init(&mutex_interrupcion, NULL);
    registros = crear_registros();
    logger = iniciar_logger("cpu.log", "CPU", argc, argv);
    config = iniciar_config("cpu.config");

    inicializar_TLB();

    linea_de_instruccion = string_new();
    linea_de_instruccion_separada = string_array_new();

    // Parte cliente, por ahora esta harcodeado, despues agregar

    cliente_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);

    // PARTE SERVIDOR
    pthread_create(&tid[DISPATCH], NULL, servidor_dispatch, NULL);
    pthread_create(&tid[INTERRUPT], NULL, servidor_interrupt, NULL);

    pthread_join(tid[DISPATCH], NULL);
    pthread_join(tid[INTERRUPT], NULL);
    // pthread_join(tid[MMU], NULL); // esto no estaba puesto, despues ver si no rompe nada 9/7/24

    liberar_conexion(cliente_memoria, logger);

    // libero todas las variables que uso
    pthread_mutex_destroy(&mutex_interrupcion);
    free(linea_de_instruccion);
    destruir_registros(registros);
    destruir_config(config);
    destruir_logger(logger);
    string_array_destroy(linea_de_instruccion_separada);
    free(TLB);
    return 0;
}

void *servidor_dispatch(void *arg)
{
    socket_servidor_dispatch = iniciar_servidor(config, "PUERTO_ESCUCHA_DISPATCH");

    log_info(logger, "Servidor Dispatch %d creado.", socket_servidor_dispatch);

    socket_cliente_dispatch = esperar_cliente(socket_servidor_dispatch, logger);

    int conexion_activa = true;
    while (conexion_activa)
    {
        int operacion = recibir_operacion(socket_cliente_dispatch, logger);
        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(socket_cliente_dispatch, logger);
            log_debug(logger, "Recibi el mensaje: %s.", mensaje);
            free(mensaje);
            break;
        case PAQUETE:
            t_list *lista = recibir_paquete(socket_cliente_dispatch, logger);
            log_trace(logger, "CPU recibe un PCB desde Kernel");
            // deberia recibir por aca un PCB para ponerme a ejercutar las instrucciones en el hilo principal
            desempaquetar_pcb_a_registros(lista, registros, logger); // aca pasa de los paquete a los registros

            list_destroy(lista); 
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", socket_cliente_dispatch);
            conexion_activa = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Recibi una operacion rara (%d), termino el servidor.", operacion);
            conexion_activa = 0;
            break;
        }
        mandar_pcb = false;
        ya_se_mando_pcb = false;
        proceso_actual_ejecutando = true;
        while (proceso_actual_ejecutando && conexion_activa)
        {
            // ciclo de la instruccion en la cpu
            int resultado = fetch();
            if (resultado == EXIT_FAILURE)
            {
                proceso_actual_ejecutando = false;
                conexion_activa = false;
                break;
            }
            decode();
            execute();
            check_interrupt();
        }

        if (mandar_pcb && !ya_se_mando_pcb && conexion_activa)
        { // hubo una interrupcion
            enviar_pcb(MOTIVO_DESALOJO_INTERRUPCION, no_agregar_datos, NULL);
        }
    }
}

void *servidor_interrupt(void *arg) // por aca va a recibir un bit cuando quiere eliminar un proceso de una
{
    socket_servidor_interrupt = iniciar_servidor(config, "PUERTO_ESCUCHA_INTERRUPT");

    log_info(logger, "Servidor Interrupt %d creado.", socket_servidor_interrupt);

    socket_cliente_interrupt = esperar_cliente(socket_servidor_interrupt, logger);

    int conexion_activa = true;
    while (conexion_activa)
    {
        int operacion = recibir_operacion(socket_cliente_interrupt, logger);
        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(socket_cliente_interrupt, logger);
            log_debug(logger, "Recibi el mensaje: %s.", mensaje);
            // no deberia haber ningun caso de este estilo

            free(mensaje); // esto no estaba puesto, despues ver si no rompe nada 9/7/24
            break;
        case PAQUETE:
            
            t_list * lista = recibir_paquete(socket_cliente_interrupt, logger);
            log_info(logger, "Se va a interrumpir el proceso con id < %d > ", list_get(lista, 0));
            // el paquete tiene el id pero no me importa, nomas ahora cambiar interrupcion a true

            // al modificar la variable global, necesito que no se interrumpa en el medio
            pthread_mutex_lock(&mutex_interrupcion);
            interrupcion = true;
            pthread_mutex_unlock(&mutex_interrupcion);

            list_destroy(lista);
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", socket_cliente_interrupt);
            conexion_activa = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Recibi una operacion rara (%d), termino el servidor.", operacion);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

e_instruccion parsear_instruccion(char *instruccionString)
{
    e_instruccion enum_instruccion;

    if (string_equals_ignore_case(instruccionString, "SET"))
    {
        enum_instruccion = SET;
    }
    else if (string_equals_ignore_case(instruccionString, "MOV_IN"))
    {
        enum_instruccion = MOV_IN;
    }
    else if (string_equals_ignore_case(instruccionString, "MOV_OUT"))
    {
        enum_instruccion = MOV_OUT;
    }
    else if (string_equals_ignore_case(instruccionString, "SUM"))
    {
        enum_instruccion = SUM;
    }
    else if (string_equals_ignore_case(instruccionString, "SUB"))
    {
        enum_instruccion = SUB;
    }
    else if (string_equals_ignore_case(instruccionString, "JNZ"))
    {
        enum_instruccion = JNZ;
    }
    else if (string_equals_ignore_case(instruccionString, "RESIZE"))
    {
        enum_instruccion = RESIZE;
    }
    else if (string_equals_ignore_case(instruccionString, "COPY_STRING"))
    {
        enum_instruccion = COPY_STRING;
    }
    else if (string_equals_ignore_case(instruccionString, "WAIT"))
    {
        enum_instruccion = WAIT;
    }
    else if (string_equals_ignore_case(instruccionString, "SIGNAL"))
    {
        enum_instruccion = SIGNAL;
    }
    else if (string_equals_ignore_case(instruccionString, "IO_GEN_SLEEP"))
    {
        enum_instruccion = IO_GEN_SLEEP;
    }
    else if (string_equals_ignore_case(instruccionString, "IO_STDIN_READ"))
    {
        enum_instruccion = IO_STDIN_READ;
    }
    else if (string_equals_ignore_case(instruccionString, "IO_STDOUT_WRITE"))
    {
        enum_instruccion = IO_STDOUT_WRITE;
    }
    else if (string_equals_ignore_case(instruccionString, "IO_FS_CREATE"))
    {
        enum_instruccion = IO_FS_CREATE;
    }
    else if (string_equals_ignore_case(instruccionString, "IO_FS_DELETE"))
    {
        enum_instruccion = IO_FS_DELETE;
    }
    else if (string_equals_ignore_case(instruccionString, "IO_FS_TRUNCATE"))
    {
        enum_instruccion = IO_FS_TRUNCATE;
    }
    else if (string_equals_ignore_case(instruccionString, "IO_FS_WRITE"))
    {
        enum_instruccion = IO_FS_WRITE;
    }
    else if (string_equals_ignore_case(instruccionString, "IO_FS_READ"))
    {
        enum_instruccion = IO_FS_READ;
    }
    else if (string_equals_ignore_case(instruccionString, "EXIT"))
    {
        enum_instruccion = INSTRUCTION_EXIT;
    }
    return enum_instruccion;
}

int fetch()
{
    // Buscamos la siguiente instruccion con el pc en memoria y la asignamos a la variable instruccion
    log_info(logger, "PID: < %d > - FETCH - Program Counter: < %d >", registros->PID, registros->PC); //OBLIGATORIO

    // creacion del paquete a enviar
    t_paquete *envioMemoria = crear_paquete();
    agregar_a_paquete(envioMemoria, OBTENER_INSTRUCCION, sizeof(OBTENER_INSTRUCCION));
    agregar_a_paquete(envioMemoria, registros->PID, sizeof(registros->PID));
    agregar_a_paquete(envioMemoria, registros->PC, sizeof(registros->PC));
    enviar_paquete(envioMemoria, cliente_memoria, logger);

    log_trace(logger, "Se envio el PID < %d > y el PC < %d > a memoria", registros->PID, registros->PC);

    // hay que recibir la operacion porque si no lee mal la instruccion, aunque no se use
    int operacion = recibir_operacion(cliente_memoria, logger);

    if (operacion == -1) // si no recibe operacion, es porque se rompio todo
    {
        return EXIT_FAILURE;
    }
    
    free(linea_de_instruccion);
    // Recibimos la instruccion de memoria

    linea_de_instruccion = recibir_mensaje(cliente_memoria, logger);
    log_debug(logger, "La instruccion leida es: < %s >", linea_de_instruccion);

    registros->PC++;
    return EXIT_SUCCESS;
}

void decode()
{

    string_array_destroy(linea_de_instruccion_separada);
    // divide la instruccion en partes y la asigna a la variable instruccion el nombre de la misma
    linea_de_instruccion_separada = string_split(linea_de_instruccion, " "); 
    instruccion = parsear_instruccion(linea_de_instruccion_separada[0]);

    // para saber si hay que hacer un uso de la MMU
    switch (instruccion)
    {
    case MOV_IN:
        // hay que hacer uso de la MMU
        linea_de_instruccion_separada[2] = componente_mmu(linea_de_instruccion_separada[2], registros->PID);
        break;
    case MOV_OUT:
        linea_de_instruccion_separada[1] = componente_mmu(linea_de_instruccion_separada[1], registros->PID);
        break;
    case COPY_STRING:
        string_array_push(&linea_de_instruccion_separada, componente_mmu("SI", registros->PID));
        string_array_push(&linea_de_instruccion_separada, componente_mmu("DI", registros->PID));
        break;
    case IO_STDIN_READ:
        linea_de_instruccion_separada[2] = componente_mmu(linea_de_instruccion_separada[2], registros->PID);
        break;
    case IO_STDOUT_WRITE:
        linea_de_instruccion_separada[2] = componente_mmu(linea_de_instruccion_separada[2], registros->PID);
        break;
    case IO_FS_WRITE:
        linea_de_instruccion_separada[3] = componente_mmu(linea_de_instruccion_separada[3], registros->PID);
        break;
    case IO_FS_READ:
        linea_de_instruccion_separada[3] = componente_mmu(linea_de_instruccion_separada[3], registros->PID);
        break;
    }

    return EXIT_SUCCESS;
}

void execute()
{
    // en base al nombre de la misma utilizamos una funcion distinta
    switch (instruccion)
    {
    case SET:
        instruction_set();
        break;
    case MOV_IN:
        instruccion_mov_in();
        break;
    case MOV_OUT:
        instruccion_mov_out();
        break;
    case SUM:
        instruccion_sum();
        break;
    case SUB:
        instruccion_sub();
        break;
    case JNZ:
        instruccion_jnz();
        break;
    case RESIZE:
        instruccion_resize();
        break;
    case COPY_STRING:
        instruccion_copy_string();
        break;
    case WAIT:
        instruccion_wait();
        break;
    case SIGNAL:
        instruccion_signal();
        break;
    case IO_GEN_SLEEP:
        instruccion_io_gen_sleep();
        break;
    case IO_STDIN_READ:
        instruccion_io_stdin_read();
        break;
    case IO_STDOUT_WRITE:
        instruccion_io_stdout_write();
        break;
    case IO_FS_CREATE:
        instruccion_io_fs_create();
        break;
    case IO_FS_DELETE:
        instruccion_io_fs_delete();
        break;
    case IO_FS_TRUNCATE:
        instruccion_io_fs_truncate();
        break;
    case IO_FS_WRITE:
        instruccion_io_fs_write();
        break;
    case IO_FS_READ:
        instruccion_io_fs_read();
        break;
    case INSTRUCTION_EXIT:
        instruccion_exit();
        break;
    default:
        log_error(logger, "instruccion incorrecta: fatal error :p");
        break;
    }
}

void instruccion_resize() 
{
    log_info(logger, "PID: < %d > - Ejecutando: < RESIZE > - < %s >", registros->PID, linea_de_instruccion_separada[1]);//OBLIGATORIO

    uint32_t nuevo_size = (uint32_t)atoi(linea_de_instruccion_separada[1]);
    // le pido a memoria que haga el resize
    log_trace(logger, "Le voy a pedir a memoria que haga un resize de %u al proceso %u.", nuevo_size, registros->PID);
    t_paquete *solicitud_a_mem = crear_paquete();
    agregar_a_paquete(solicitud_a_mem, RESIZE_PROCESO, sizeof(RESIZE_PROCESO));
    agregar_a_paquete(solicitud_a_mem, nuevo_size, sizeof(nuevo_size));
    agregar_a_paquete(solicitud_a_mem, registros->PID, sizeof(registros->PID));
    enviar_paquete(solicitud_a_mem, cliente_memoria, logger);

    // espero la respuesta de memoria
    recibir_operacion(cliente_memoria, logger); // Se rompe si no recibe operacion
    t_list *lista_de_memoria = recibir_paquete(cliente_memoria, logger);
    uint8_t resultado = list_get(lista_de_memoria, 0);

    log_debug(logger, "Resultado del resize: %s", (resultado ? "Out of Memory" : "Ejecutado correctamente"));

    // si hubo un out of memory, le aviso a kernel
    if (resultado)
    {
        enviar_pcb(MOTIVO_DESALOJO_OUT_OF_MEMORY, no_agregar_datos, NULL);
        proceso_actual_ejecutando = false; // https://github.com/sisoputnfrba/foro/issues/3799
        ya_se_mando_pcb = true;
    }

    //Liberacion de memoria
    list_destroy(lista_de_memoria); 
}

void instruccion_io_fs_create()
{
    log_info(logger, "PID: < %d > - Ejecutando: < FS CREATE > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]);//OBLIGATORIO
    char *nombre_interfaz = string_duplicate(linea_de_instruccion_separada[1]);
    char *nombre_archivo = string_duplicate(linea_de_instruccion_separada[2]);

    char **datos_interfaz = string_array_new();
    string_array_push(&datos_interfaz, nombre_interfaz);
    string_array_push(&datos_interfaz, nombre_archivo);
    enviar_pcb(MOTIVO_DESALOJO_IO_FS_CREATE, agregar_datos_interfaz_create_delete, datos_interfaz);

    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;

    string_array_destroy(datos_interfaz); 
}

void instruccion_io_fs_delete()
{
    log_info(logger, "PID: < %d > - Ejecutando: < FS DELETE > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]);
    char *nombre_interfaz = string_duplicate(linea_de_instruccion_separada[1]);
    char *nombre_archivo = string_duplicate(linea_de_instruccion_separada[2]);

    char **datos_interfaz = string_array_new();
    string_array_push(&datos_interfaz, nombre_interfaz);
    string_array_push(&datos_interfaz, nombre_archivo);
    enviar_pcb(MOTIVO_DESALOJO_IO_FS_DELETE, agregar_datos_interfaz_create_delete, datos_interfaz);

    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;
    string_array_destroy(datos_interfaz); 
}
void instruccion_io_fs_truncate()
{
    log_info(logger, "PID: < %d > - Ejecutando: < FS TRUNCATE > - < %s %s %s>", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2], linea_de_instruccion_separada[3]);
    char *nombre_interfaz = string_duplicate(linea_de_instruccion_separada[1]);
    char *nombre_archivo = string_duplicate(linea_de_instruccion_separada[2]);
    char *nombre_registro = string_duplicate(linea_de_instruccion_separada[3]);
    char *valor_registro_string = string_itoa(obtenerValorRegistros(nombre_registro));

    char **datos_interfaz = string_array_new();
    string_array_push(&datos_interfaz, nombre_interfaz);
    string_array_push(&datos_interfaz, nombre_archivo);
    string_array_push(&datos_interfaz, valor_registro_string);
    enviar_pcb(MOTIVO_DESALOJO_IO_FS_TRUNCATE, agregar_datos_interfaz_truncate, datos_interfaz);

    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;
    string_array_destroy(datos_interfaz); 
    free(nombre_registro);
}

void instruccion_io_fs_read()
{
    log_info(logger, "PID: < %d > - Ejecutando: < FS READ > - < %s %s %s %s %s>", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2], linea_de_instruccion_separada[3], linea_de_instruccion_separada[4], linea_de_instruccion_separada[5]);
    char *nombre_interfaz = string_duplicate(linea_de_instruccion_separada[1]);
    char *nombre_archivo = string_duplicate(linea_de_instruccion_separada[2]);
    char *df = string_duplicate(linea_de_instruccion_separada[3]);
    char *nombre_registro_tamanio = string_duplicate(linea_de_instruccion_separada[4]);
    char *valor_tamanio_string = string_itoa(obtenerValorRegistros(nombre_registro_tamanio));
    char *nombre_registro_puntero_archivo = string_duplicate(linea_de_instruccion_separada[5]);
    char *valor_puntero_archivo_string = string_itoa(obtenerValorRegistros(nombre_registro_puntero_archivo));

    char **datos_interfaz = string_array_new();
    string_array_push(&datos_interfaz, nombre_interfaz);
    string_array_push(&datos_interfaz, nombre_archivo);
    string_array_push(&datos_interfaz, df);
    string_array_push(&datos_interfaz, valor_tamanio_string);
    string_array_push(&datos_interfaz, valor_puntero_archivo_string);
    enviar_pcb(MOTIVO_DESALOJO_IO_FS_READ, agregar_datos_interfaz_read_write, datos_interfaz);

    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;
    string_array_destroy(datos_interfaz); 
    free(nombre_registro_tamanio);
    free(nombre_registro_puntero_archivo);
}

void instruccion_io_fs_write()
{
    log_info(logger, "PID: < %d > - Ejecutando: < FS WRITE > - < %s %s %s %s %s>", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2], linea_de_instruccion_separada[3], linea_de_instruccion_separada[4], linea_de_instruccion_separada[5]);
    char *nombre_interfaz = string_duplicate(linea_de_instruccion_separada[1]);
    char *nombre_archivo = string_duplicate(linea_de_instruccion_separada[2]);
    char *df = string_duplicate(linea_de_instruccion_separada[3]);
    char *nombre_registro_tamanio = string_duplicate(linea_de_instruccion_separada[4]);
    char *valor_tamanio_string = string_itoa(obtenerValorRegistros(nombre_registro_tamanio));
    char *nombre_registro_puntero_archivo = string_duplicate(linea_de_instruccion_separada[5]);
    char *valor_puntero_archivo_string = string_itoa(obtenerValorRegistros(nombre_registro_puntero_archivo));

    char **datos_interfaz = string_array_new();
    string_array_push(&datos_interfaz, nombre_interfaz);
    string_array_push(&datos_interfaz, nombre_archivo);
    string_array_push(&datos_interfaz, df);
    string_array_push(&datos_interfaz, valor_tamanio_string);
    string_array_push(&datos_interfaz, valor_puntero_archivo_string);
    enviar_pcb(MOTIVO_DESALOJO_IO_FS_WRITE, agregar_datos_interfaz_read_write, datos_interfaz);

    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;
    string_array_destroy(datos_interfaz); 
    free(nombre_registro_puntero_archivo);
    free(nombre_registro_tamanio);
}

void instruccion_io_stdin_read()
{
    log_info(logger, "PID: < %d > - Ejecutando: < IO_STDIN_READ > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]);
    char *df = string_duplicate(linea_de_instruccion_separada[2]);
    char *nombreInterfaz = string_duplicate(linea_de_instruccion_separada[1]);
    char *tamanio = string_itoa(obtenerValorRegistros(linea_de_instruccion_separada[3]));

    char **datos_interfaz_stdin_read = string_array_new();
    string_array_push(&datos_interfaz_stdin_read, df);
    string_array_push(&datos_interfaz_stdin_read, nombreInterfaz);
    string_array_push(&datos_interfaz_stdin_read, tamanio);

    enviar_pcb(MOTIVO_DESALOJO_IO_STDIN_READ, agregar_datos_interfaz_std, datos_interfaz_stdin_read);

    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;
    string_array_destroy(datos_interfaz_stdin_read);
}

void instruccion_io_stdout_write()
{
    log_info(logger, "PID: < %d > - Ejecutando: < IO_STDOUT_WRITE > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]);
    char *df = string_duplicate(linea_de_instruccion_separada[2]);
    char *nombreInterfaz = string_duplicate(linea_de_instruccion_separada[1]);
    char *tamanio = string_itoa(obtenerValorRegistros(linea_de_instruccion_separada[3]));

    char **datos_interfaz_std = string_array_new();
    string_array_push(&datos_interfaz_std, df);
    string_array_push(&datos_interfaz_std, nombreInterfaz);
    string_array_push(&datos_interfaz_std, tamanio);

    enviar_pcb(MOTIVO_DESALOJO_IO_STDOUT_WRITE, agregar_datos_interfaz_std, datos_interfaz_std);

    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;

    string_array_destroy(datos_interfaz_std);
}

void instruction_set()
{
    log_info(logger, "PID: < %d > - Ejecutando: < SET > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]);

    char *registroDestino = string_duplicate(linea_de_instruccion_separada[1]);

    char *valorEnString = string_duplicate(linea_de_instruccion_separada[2]);

    int valorEnInt = atoi(valorEnString);

    // Establecer el valor en el registro correspondiente
    asignarValoresIntEnRegistros(registroDestino, valorEnInt, "SET");

    free(registroDestino);
    free(valorEnString);
}

void instruccion_sum()
{
    log_info(logger, "PID: < %d > - Ejecutando: < SUM > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]);

    char *registroDestino = string_duplicate(linea_de_instruccion_separada[1]);

    char *registroOrigen = string_duplicate(linea_de_instruccion_separada[2]);

    // Obtengo el valor almacenado en el registro uint8_t
    int valorDestino = obtenerValorRegistros(registroDestino);
    int valorOrigen = obtenerValorRegistros(registroOrigen);
    int valorFinal = valorOrigen + valorDestino;

    asignarValoresIntEnRegistros(registroDestino, valorFinal, "SUM");
    free(registroDestino);
    free(registroOrigen);
}

void instruccion_jnz()
{
    log_info(logger, "PID: < %d > - Ejecutando: < JNZ > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]);

    char* registroDestino = string_duplicate(linea_de_instruccion_separada[1]);

    char* parametro = string_duplicate(linea_de_instruccion_separada[2]);

    int nuevoPC = atoi(parametro) - 1;

    int valorDelRegistro = obtenerValorRegistros(registroDestino);
    if (valorDelRegistro != 0)
    {
        registros->PC = (uint32_t)nuevoPC;
        log_trace(logger, "Se actualizo el PC a la instruccion nro: %d", nuevoPC);
    }

    free(registroDestino);
    free(parametro);
}

void instruccion_sub()
{
    log_info(logger, "PID: < %d > - Ejecutando: < SUB > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]);

    char *registroDestino = string_duplicate(linea_de_instruccion_separada[1]);

    char *registroOrigen = string_duplicate(linea_de_instruccion_separada[2]);

    // Obtengo el valor almacenado en el registro uint8_t
    int valorOrigen = obtenerValorRegistros(registroOrigen);
    int valorDestino = obtenerValorRegistros(registroDestino);
    int valorFinal = valorDestino - valorOrigen;

    asignarValoresIntEnRegistros(registroDestino, valorFinal, "SUB");
    free(registroDestino);
    free(registroOrigen);
}

void instruccion_wait()
{
    // envio el pcb a kernel y luego un mensaje con el nombre del recurso que quiero
    // Esta instrucción solicita al Kernel que se asigne una instancia del recurso indicado por parámetro.
    log_info(logger, "PID: < %d > - Ejecutando: < WAIT > - < %s >", registros->PID, linea_de_instruccion_separada[1]);
    char *nombre_recurso = string_duplicate(linea_de_instruccion_separada[1]);

    enviar_pcb(MOTIVO_DESALOJO_WAIT, agregar_datos_recurso, nombre_recurso);

    // Espero la respuesta de Kernel
    
    recibir_operacion(socket_cliente_dispatch, logger);
    t_list *resp_kernel_wait = recibir_paquete(socket_cliente_dispatch, logger);

    uint8_t resultado_numero = list_get(resp_kernel_wait, 0);
    // En nuestro conversacion con kernel si es 0 consigio el recurso y 1 significa que, o no existe, o no me lo puede dar
    if (resultado_numero == 1)
    {
        log_debug(logger, "No se pudo obtener el recurso < %s >", nombre_recurso);
        proceso_actual_ejecutando = false;
        ya_se_mando_pcb = true;
    }

    list_destroy(resp_kernel_wait);
    free(nombre_recurso);
}

void instruccion_signal()
{
    // Esta instrucción solicita al Kernel que se libere una instancia del recurso indicado por parámetro.
    log_info(logger, "PID: < %d > - Ejecutando: < SIGNAL > - < %s >", registros->PID, linea_de_instruccion_separada[1]);

    char* nombre_recurso = string_duplicate(linea_de_instruccion_separada[1]);

    enviar_pcb(MOTIVO_DESALOJO_SIGNAL, agregar_datos_recurso, nombre_recurso);

    // Espero la respuesta de Kernel
    recibir_operacion(socket_cliente_dispatch, logger);
    t_list *resp_kernel_signal = recibir_paquete(socket_cliente_dispatch, logger);

    uint8_t resultado_numero = list_get(resp_kernel_signal, 0);

    if (resultado_numero)
    {
        log_debug(logger, "No se pudo obtener el recurso < %s >", nombre_recurso);
        proceso_actual_ejecutando = false;
        ya_se_mando_pcb = true;
    }

    free(nombre_recurso);
    list_destroy(resp_kernel_signal);
}

void instruccion_io_gen_sleep()
{
    log_info(logger, "PID: < %d > - Ejecutando: < IO_GEN_SLEEP > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]); //OBLIGATORIO

    char * nroInterfaz = string_duplicate(linea_de_instruccion_separada[1]);


    char * tiempoTrabajoString = string_duplicate(linea_de_instruccion_separada[2]);

    char **datos_tiempo = string_array_new();
    string_array_push(&datos_tiempo, nroInterfaz);
    string_array_push(&datos_tiempo, tiempoTrabajoString);

    enviar_pcb(MOTIVO_DESALOJO_IO_GEN_SLEEP, agregar_datos_tiempo, datos_tiempo);
    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;

    // LIBERAR LA MEMORIA
    string_array_destroy(datos_tiempo);
}

void instruccion_copy_string() // "funciona .-."
{
    log_info(logger, "PID: < %d > - Ejecutando: < COPY_STRING > - < %s >", registros->PID, linea_de_instruccion_separada[1]); //OBLIGATORIO

    uint32_t tamanio = (uint32_t)atoi(linea_de_instruccion_separada[1]);       // tamanio a copiar
    uint32_t df_a_leer = (uint32_t)atoi(linea_de_instruccion_separada[2]);     // direccion de memoria a leer
    uint32_t df_a_escribir = (uint32_t)atoi(linea_de_instruccion_separada[3]); // direccion de memoria a escribir
    log_debug(logger, "Direccion fisica de lectura: %u, Direccion fisica de escritura: %u, Tamanio a copiar: %u", df_a_leer,df_a_escribir,tamanio);
    // hay que hacer un pedido lectura y escritura a memoria
    t_paquete *paquete_lectura = crear_paquete();
    agregar_a_paquete(paquete_lectura, PEDIDO_LECTURA, sizeof(PEDIDO_LECTURA));
    agregar_a_paquete(paquete_lectura, df_a_leer, sizeof(df_a_leer));
    agregar_a_paquete(paquete_lectura, tamanio, sizeof(tamanio));
    agregar_a_paquete(paquete_lectura, registros->PID, sizeof(registros->PID));
    agregar_a_paquete(paquete_lectura, 0, sizeof(uint8_t));   // El 0 es define el tipo char*
    enviar_paquete(paquete_lectura, cliente_memoria, logger); // Enviamos el paquete para la lectura en memoria
    recibir_operacion(cliente_memoria, logger);
    t_list *lista_de_memoria = recibir_paquete(cliente_memoria, logger);
    char *string_a_copiar = list_get(lista_de_memoria, 0);
    log_debug(logger, "String a copiar: %s", string_a_copiar);

    t_paquete *paquete_escritura = crear_paquete();
    agregar_a_paquete(paquete_escritura, PEDIDO_ESCRITURA, sizeof(PEDIDO_ESCRITURA));
    agregar_a_paquete(paquete_escritura, df_a_escribir, sizeof(df_a_escribir));
    agregar_a_paquete(paquete_escritura, tamanio, sizeof(tamanio));
    agregar_a_paquete(paquete_escritura, registros->PID, sizeof(registros->PID));
    agregar_a_paquete(paquete_escritura, string_a_copiar, string_length(string_a_copiar) + 1); // si rompe fijarse aca si es eso o un string len +1
    agregar_a_paquete(paquete_escritura, 0, sizeof(uint8_t));
    enviar_paquete(paquete_escritura, cliente_memoria, logger);
    recibir_operacion(cliente_memoria, logger);
    char *resultado = recibir_mensaje(cliente_memoria, logger);
    if (strcmp(resultado, "OK"))
    {
        log_trace(logger, "Se copio correctamente el string");
    }

    //Liberacion de memoria
    free(resultado);
    list_destroy(lista_de_memoria);
    free(string_a_copiar);
}

void instruccion_mov_in()
{
    log_info(logger, "PID: < %d > - Ejecutando: < MOV_IN > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]); //OBLIGATORIO
    // hacer un pedido de lectura
    uint32_t df_a_leer = (uint32_t)atoi(linea_de_instruccion_separada[2]); // direccion de memoria a leer
    uint32_t nuevo_size = obtener_size_del_registro(linea_de_instruccion_separada[1]);
    t_paquete *paquete_lectura = crear_paquete();
    agregar_a_paquete(paquete_lectura, PEDIDO_LECTURA, sizeof(PEDIDO_LECTURA));
    agregar_a_paquete(paquete_lectura, df_a_leer, sizeof(df_a_leer));
    agregar_a_paquete(paquete_lectura, nuevo_size, sizeof(nuevo_size));
    agregar_a_paquete(paquete_lectura, registros->PID, sizeof(registros->PID));
    agregar_a_paquete(paquete_lectura, 1, sizeof(uint8_t));   // El 1 porque es un numero|
    enviar_paquete(paquete_lectura, cliente_memoria, logger); // Enviamos el paquete para la lectura en memoria
    recibir_operacion(cliente_memoria, logger);
    t_list *lista_de_memoria = recibir_paquete(cliente_memoria, logger);
    uint32_t valor = (uint32_t)list_get(lista_de_memoria, 0);
    asignarValoresIntEnRegistros(linea_de_instruccion_separada[1], valor, "MOV_IN");

    //Liberaciones de memoria
    list_destroy(lista_de_memoria); // esto no estaba puesto, despues ver si no rompe nada 9/7/24
}

void instruccion_mov_out()
{
    log_info(logger, "PID: < %d > - Ejecutando: < MOV_OUT > - < %s %s >", registros->PID, linea_de_instruccion_separada[1], linea_de_instruccion_separada[2]); //OBLIGATORIO
    uint32_t valorRegistro = obtenerValorRegistros(linea_de_instruccion_separada[2]);
    uint32_t nuevo_size = obtener_size_del_registro(linea_de_instruccion_separada[2]);
    uint32_t df_a_escribir = (uint32_t)atoi(linea_de_instruccion_separada[1]);
    t_paquete *paquete_escritura = crear_paquete();
    agregar_a_paquete(paquete_escritura, PEDIDO_ESCRITURA, sizeof(PEDIDO_ESCRITURA));
    agregar_a_paquete(paquete_escritura, df_a_escribir, sizeof(df_a_escribir));
    agregar_a_paquete(paquete_escritura, nuevo_size, sizeof(nuevo_size));
    agregar_a_paquete(paquete_escritura, registros->PID, sizeof(registros->PID));
    agregar_a_paquete(paquete_escritura, valorRegistro, sizeof(valorRegistro));
    agregar_a_paquete(paquete_escritura, 1, sizeof(uint8_t)); // El 1 porque es un numero
    enviar_paquete(paquete_escritura, cliente_memoria, logger);
    recibir_operacion(cliente_memoria, logger);
    char *resultado = recibir_mensaje(cliente_memoria, logger);
    if (strcmp(resultado, "OK"))
    {
        log_trace(logger, "Se copio correctamente el registro");
    }

    //Liberacion de memoria
    free(resultado); // esto no estaba puesto, despues ver si no rompe nada 9/7/24
}

void instruccion_exit()
{
    log_info(logger, "PID: < %d > - Ejecutando: < EXIT > - < >", registros->PID); //OBLIGATORIO
    // deberia mandar el pcb a kernel
    enviar_pcb(MOTIVO_DESALOJO_EXIT, no_agregar_datos, NULL);
    ya_se_mando_pcb = true;
    proceso_actual_ejecutando = false;
}

void asignarValoresIntEnRegistros(char *registroDestino, int valor, char *instruccion)
{
    if (strcmp(registroDestino, "AX") == 0)
    {
        registros->AX = (uint8_t)valor;
    }
    else if (strcmp(registroDestino, "BX") == 0)
    {
        registros->BX = (uint8_t)valor;
    }
    else if (strcmp(registroDestino, "CX") == 0)
    {
        registros->CX = (uint8_t)valor;
    }
    else if (strcmp(registroDestino, "DX") == 0)
    {
        registros->DX = (uint8_t)valor;
    }
    else if (strcmp(registroDestino, "EAX") == 0)
    {
        registros->EAX = (uint32_t)valor;
    }
    else if (strcmp(registroDestino, "EBX") == 0)
    {
        registros->EBX = (uint32_t)valor;
    }
    else if (strcmp(registroDestino, "ECX") == 0)
    {
        registros->ECX = (uint32_t)valor;
    }
    else if (strcmp(registroDestino, "EDX") == 0)
    {
        registros->EDX = (uint32_t)valor;
    }
    else if (strcmp(registroDestino, "SI") == 0)
    {
        registros->SI = (uint32_t)valor;
    }
    else if (strcmp(registroDestino, "DI") == 0)
    {
        registros->DI = (uint32_t)valor;
    }
    else if (strcmp(registroDestino, "PC") == 0)
    {
        registros->PC = (uint32_t)valor;
    }
    else
    {
        log_error(logger, "Hubo un error al ejecutar la instruccion %s", instruccion);
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int obtenerValorRegistros(char *registroCPU)
{
    if (strcmp(registroCPU, "AX") == 0)
    {
        return (int)registros->AX;
    }
    else if (strcmp(registroCPU, "BX") == 0)
    {
        return (int)registros->BX;
    }
    else if (strcmp(registroCPU, "CX") == 0)
    {
        return (int)registros->CX;
    }
    else if (strcmp(registroCPU, "DX") == 0)
    {
        return (int)registros->DX;
    }
    else if (strcmp(registroCPU, "EAX") == 0)
    {
        return (int)registros->EAX;
    }
    else if (strcmp(registroCPU, "EBX") == 0)
    {
        return (int)registros->EBX;
    }
    else if (strcmp(registroCPU, "ECX") == 0)
    {
        return (int)registros->ECX;
    }
    else if (strcmp(registroCPU, "EDX") == 0)
    {
        return (int)registros->EDX;
    }
    else if (strcmp(registroCPU, "SI") == 0)
    {
        return (int)registros->SI;
    }
    else if (strcmp(registroCPU, "DI") == 0)
    {
        return (int)registros->DI;
    }
    else
    {
        log_error(logger, "NO SE PUDO OBTENER EL VALOR DEL REGISTRO: %s", registroCPU);
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int obtener_size_del_registro(char *registroCPU)
{
    if (strcmp(registroCPU, "AX") == 0)
    {
        return sizeof(registros->AX);
    }
    else if (strcmp(registroCPU, "BX") == 0)
    {
        return sizeof(registros->BX);
    }
    else if (strcmp(registroCPU, "CX") == 0)
    {
        return sizeof(registros->CX);
    }
    else if (strcmp(registroCPU, "DX") == 0)
    {
        return sizeof(registros->DX);
    }
    else if (strcmp(registroCPU, "EAX") == 0)
    {
        return sizeof(registros->EAX);
    }
    else if (strcmp(registroCPU, "EBX") == 0)
    {
        return sizeof(registros->EBX);
    }
    else if (strcmp(registroCPU, "ECX") == 0)
    {
        return sizeof(registros->ECX);
    }
    else if (strcmp(registroCPU, "EDX") == 0)
    {
        return sizeof(registros->EDX);
    }
    else if (strcmp(registroCPU, "SI") == 0)
    {
        return sizeof(registros->SI);
    }
    else if (strcmp(registroCPU, "DI") == 0)
    {
        return sizeof(registros->DI);
    }
    else
    {
        log_error(logger, "NO SE PUDO OBTENER EL SIZE DEL REGISTRO: %s", registroCPU);
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

void check_interrupt()
{
    pthread_mutex_lock(&mutex_interrupcion);
    if (interrupcion)
    {
        log_trace(logger, "Se recibio una interrupcion");
        mandar_pcb = true;
        proceso_actual_ejecutando = false;
        interrupcion = false;
    }
    pthread_mutex_unlock(&mutex_interrupcion);
}

void enviar_pcb(e_motivo_desalojo motivo_desalojo, Agregar_datos_paquete agregar_datos_paquete, void *datos)
{
    log_trace(logger, "ENVIAR_PCB: CPU va a enviar un PCB a Kernel");
    registros->motivo_desalojo = motivo_desalojo;
    t_paquete *paquete_de_pcb = crear_paquete();
    empaquetar_registros(paquete_de_pcb, registros);
    agregar_datos_paquete(paquete_de_pcb, datos);
    enviar_paquete(paquete_de_pcb, socket_cliente_dispatch, logger);
}

void agregar_datos_recurso(t_paquete *paquete, void *nombre_recurso)
{
    // en este caso datos va a ser el nombre del recurso que voy a pedir o devolver
    agregar_a_paquete(paquete, nombre_recurso, strlen(nombre_recurso) + 1);
}

void agregar_datos_interfaz_std(t_paquete *paquete, void *datos)
{
    char **datos_interfaz_stdin_read = (char **)datos;
    // convierto el tamanio y el df a uint32_t
    uint32_t tamanio = (uint32_t)atoi(datos_interfaz_stdin_read[2]);
    uint32_t df = (uint32_t)atoi(datos_interfaz_stdin_read[0]);
    char *nombre_interfaz = datos_interfaz_stdin_read[1];
    agregar_a_paquete(paquete, nombre_interfaz, strlen(nombre_interfaz) + 1);
    agregar_a_paquete(paquete, df, sizeof(df));
    agregar_a_paquete(paquete, tamanio, sizeof(tamanio));
}

void agregar_datos_interfaz_create_delete(t_paquete *paquete, void *datos)
{
    char **datos_interfaz = (char **)datos;
    char *nombre_interfaz = datos_interfaz[0];
    char *nombre_archivo = datos_interfaz[1];
    agregar_a_paquete(paquete, nombre_interfaz, strlen(nombre_interfaz) + 1);
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);
}

void agregar_datos_interfaz_truncate(t_paquete *paquete, void *datos)
{
    char **datos_interfaz = (char **)datos;
    char *nombre_interfaz = datos_interfaz[0];
    char *nombre_archivo = datos_interfaz[1];
    uint32_t tamanio = (uint32_t)atoi(datos_interfaz[2]);
    agregar_a_paquete(paquete, nombre_interfaz, strlen(nombre_interfaz) + 1);
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);
    agregar_a_paquete(paquete, tamanio, sizeof(tamanio));
}

void agregar_datos_interfaz_read_write(t_paquete *paquete, void *datos)
{
    char **datos_interfaz = (char **)datos;
    char *nombre_interfaz = datos_interfaz[0];
    char *nombre_archivo = datos_interfaz[1];
    uint32_t df = (uint32_t)atoi(datos_interfaz[2]);
    uint32_t tamanio = (uint32_t)atoi(datos_interfaz[3]);
    uint32_t puntero_archivo = (uint32_t)atoi(datos_interfaz[4]);
    agregar_a_paquete(paquete, nombre_interfaz, strlen(nombre_interfaz) + 1);
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);
    agregar_a_paquete(paquete, df, sizeof(df));
    agregar_a_paquete(paquete, tamanio, sizeof(tamanio));
    agregar_a_paquete(paquete, puntero_archivo, sizeof(puntero_archivo));
}

void agregar_datos_tiempo(t_paquete *paquete, void *datos)
{
    // en este caso datos tiene dos cosas el numero de interfaz y el tiempo de trabajo
    log_trace(logger, "Estoy en la funcion agregar_datos_tiempo");

    char **datos_tiempo = (char **)datos;

    agregar_a_paquete(paquete, datos_tiempo[0], strlen(datos_tiempo[0]) + 1); // Agregas al paquete el nroInterfaz

    uint32_t timeClock = (uint32_t)atoi(datos_tiempo[1]);

    agregar_a_paquete(paquete, timeClock, sizeof(timeClock)); // Agregas al paquete el tiempo
}

void no_agregar_datos(t_paquete *paquete, void *datos)
{
    log_trace(logger, "Estoy en la funcion no_agregar_datos");
}

// MMU
char *componente_mmu(char *registro, uint32_t pid)
{
    // conseguimos lo que esta dentro del registro
    uint32_t dl = (uint32_t)obtenerValorRegistros(registro);
    uint32_t nro_pagina = floor(dl / tam_pag);
    uint32_t desplazamiento = dl - (nro_pagina * tam_pag);
    log_debug(logger, "PID: < %u > - Direccion Logica: < %u > - Nro Pagina: < %u > - Desplazamiento: < %u >", pid, dl, nro_pagina, desplazamiento);
    int marco = conseguir_marco(pid, nro_pagina);
    // harcodea que el marco es igual a la pagina
    // int marco = (int) nro_pagina;
    uint32_t direccion_fisica = (uint32_t)(marco * tam_pag) + desplazamiento;
    log_debug(logger, "Direccion Fisica: %d", direccion_fisica);
    return (string_itoa(direccion_fisica));
}

// Hacemos chanchadas para conseguir el tam_pag
int conseguir_tam_pag()
{
    config = iniciar_config("../memoria/memoria.config");
    int tam_pag = config_get_int_value(config, "TAM_PAGINA");
    destruir_config(config);
    return tam_pag;
}

int conseguir_marco(uint32_t pid, uint32_t nro_pagina)
{

    int marco = conseguir_marco_en_la_tlb(pid, nro_pagina);
    if (marco == -1)
    {

        // si no esta en la tlb, tengo que pedirlo a memoria
        t_paquete *paquete = crear_paquete();
        agregar_a_paquete(paquete, OBTENER_MARCO, sizeof(OBTENER_MARCO));
        agregar_a_paquete(paquete, pid, sizeof(pid));
        agregar_a_paquete(paquete, nro_pagina, sizeof(nro_pagina));
        enviar_paquete(paquete, cliente_memoria, logger);

        // recibir el marco
        recibir_operacion(cliente_memoria, logger);
        t_list *lista_de_memoria = recibir_paquete(cliente_memoria, logger);
        marco = (int)list_get(lista_de_memoria, 0);
        agregar_a_tlb(pid, nro_pagina, marco);
        instante_referencia++;
        list_destroy(lista_de_memoria);
    }
    log_info(logger, "PID: < %u > - OBTENER MARCO - Página: < %u > - Marco: < %u > ", pid, nro_pagina, marco); //OBLIGATORIO
    return marco;
}

void inicializar_TLB()
{
    tlb_size = config_get_int_value(config, "CANTIDAD_ENTRADAS_TLB");
    TLB = malloc(tlb_size * sizeof(t_TLB));
    for (int i = 0; i < tlb_size; i++)
    {
        TLB[i].PID = -1;
    }
}

int conseguir_marco_en_la_tlb(uint32_t pid, uint32_t nro_pagina)
{
    for (int i = 0; i < tlb_size; i++)
    {
        if (TLB[i].PID == (int)pid && TLB[i].nro_pag == nro_pagina)
        { 
            log_info(logger, "PID: < %u > - TLB HIT - Pagina: < %u > ", pid, nro_pagina); //OBLIGATORIO
            TLB[i].instante_refencia = instante_referencia;
            instante_referencia++;
            return ((int)TLB[i].nro_marco);
        }
    }
    log_info(logger, "PID: < %u > - TLB MISS - Pagina: < %u > ", pid, nro_pagina); //OBLIGATORIO
    return -1;
}

int agregar_a_tlb(uint32_t pid, uint32_t nro_pagina, uint32_t nro_marco)
{
    if (tlb_size == 0)
    {
        return EXIT_FAILURE;
    }
    for (int i = 0; i < tlb_size; i++)
    {
        // si encuentra un hueco libre, lo agrega ahi
        if (TLB[i].PID == -1)
        {
            TLB[i].PID = (int)pid;
            TLB[i].nro_pag = nro_pagina;
            TLB[i].nro_marco = nro_marco;
            TLB[i].instante_refencia = instante_referencia;
            return EXIT_SUCCESS;
        }
    }
    // si no hay hueco libre, lo agrega en la primera posicion que se uso
    log_debug(logger, "TLB LLENA - Se va a reemplazar una entrada");
    char *algortimo_tlb = string_duplicate(config_get_string_value(config, "ALGORITMO_TLB"));
    if (string_equals_ignore_case(algortimo_tlb, "FIFO"))
    {
        if (ultima_fila_modificada == tlb_size)
        {
            ultima_fila_modificada = 0;
        }
        log_info(logger, "TLB - Se remplaza una entrada en el indice %d", ultima_fila_modificada);
        TLB[ultima_fila_modificada].PID = (int)pid;
        TLB[ultima_fila_modificada].nro_pag = nro_pagina;
        TLB[ultima_fila_modificada].nro_marco = nro_marco;
        TLB[ultima_fila_modificada].instante_refencia = instante_referencia;
        ultima_fila_modificada++;
    }
    else
    {
        // LRU
        int indice = buscar_tlb_con_menor_referencia();
        log_info(logger, "TLB - Se remplaza una entrada en el indice %d", indice);
        TLB[indice].PID = (int)pid;
        TLB[indice].nro_pag = nro_pagina;
        TLB[indice].nro_marco = nro_marco;
        TLB[indice].instante_refencia = instante_referencia;
    }
    free(algortimo_tlb);
    return EXIT_FAILURE;
}

int buscar_tlb_con_menor_referencia()
{
    int menor = TLB[0].instante_refencia;
    int indice = 0;
    for (int i = 1; i < tlb_size; i++)
    {
        if (TLB[i].instante_refencia < menor)
        {
            menor = TLB[i].instante_refencia;
            indice = i;
        }
    }
    return indice;
}