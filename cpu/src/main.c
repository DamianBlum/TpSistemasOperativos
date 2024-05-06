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

// hilos
pthread_t tid[2];

// registros 
t_registros *registros;


// bit para la interrupion
bool interrupcion;

// mutex para la variable interrupcion
pthread_mutex_t mutex_interrupcion = PTHREAD_MUTEX_INITIALIZER;
int interrupcion_init;

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

int main(int argc, char *argv[])
{
    // Inicializo las variables
    interrupcion = false;
    interrupcion_init = pthread_mutex_init(&mutex_interrupcion, NULL);
    registros = crear_registros();
    logger = iniciar_logger("cpu.log", "CPU", argc, argv);
    config = iniciar_config("cpu.config");

    linea_de_instruccion = string_new();   
    linea_de_instruccion_separada = string_array_new();
    
    // Parte cliente, por ahora esta harcodeado, despues agregar
    
    cliente_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
    

    
    // PARTE SERVIDOR

    pthread_create(&tid[DISPATCH], NULL, servidor_dispatch, NULL);
    pthread_create(&tid[INTERRUPT], NULL, servidor_interrupt, NULL);
    
    
    pthread_join(tid[DISPATCH], NULL);
    pthread_join(tid[INTERRUPT], NULL);


    liberar_conexion(cliente_memoria, logger);
    
    // libero todas las variables que uso
    pthread_mutex_destroy(&mutex_interrupcion);
    free(linea_de_instruccion);
    destruir_registros(registros);
    destruir_config(config);
    destruir_logger(logger);
    string_array_destroy(linea_de_instruccion_separada);
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
                log_info(logger, "Recibi el mensaje: %s.", mensaje);
                free(mensaje);
                break;
            case PAQUETE:
                t_list *lista = list_create();
                lista = recibir_paquete(socket_cliente_dispatch, logger);
                log_trace(logger, "CPU recibe un PCB desde Kernel");
                // deberia recibir por aca un PCB para ponerme a ejercutar las instrucciones en el hilo principal  
                desempaquetar_pcb_a_registros(lista,registros,logger); //aca pasa de los paquete a los registros
                //list_destroy_and_destroy_elements(lista); 
                break;
            case EXIT: // indica desconeccion
                log_error(logger, "Se desconecto el cliente %d.", socket_cliente_dispatch);
                conexion_activa = 0;
                break;
            default: // recibi algo q no es eso, vamos a suponer q es para terminar
                log_error(logger, "Recibi una operacion rara (%d), termino el servidor.", operacion);
                return EXIT_FAILURE;
        }
        mandar_pcb = false;
        proceso_actual_ejecutando = true;
        while (proceso_actual_ejecutando) {

            fetch(); 
            log_debug(logger, "LOG DESPUES DEL FETCH: %s", linea_de_instruccion);
            //busca con registros->PC en la memoria 
            //y devuelve un string por ahora me imagine con una instruccion de esta forma "SUM AX BX" 
            decode(); 
            log_debug(logger, "LOG ANTES DEL execute: %s", linea_de_instruccion_separada[0]);
            log_debug(logger, "LOG ANTES DEL execute: %s", linea_de_instruccion_separada[1]);
            log_debug(logger, "LOG ANTES DEL execute: %s", linea_de_instruccion_separada[2]);
            //consigue la primera palabra que seria el "SUM" y en base a eso en un switch o algo de ese estilo despues ve que seria "AX" y "BX"
            execute(); 
            log_debug(logger, "VALOR DEL REGISTRO AX: %d ", registros->AX );
            //Ejecuta la instruccion y guarda la data en los registros que se actualizaro y aumenta el PC en uno si no fue un EXIT
            // a la vez si es exucute de un exit deberia finalizar la ejecucion de este proceso no se como
            // write back es como usar el log, ahi se actualiza el PC en uno
            check_interrupt();
            // se fija si esta la interrupcion de eliminar_proceso
            // y el cpu carga despues nuevo_registros
            //recordar que las unicas interrupciones son o por quantum o por entrada salida que pida la instruccion!
            // la unica interrupcion exterena es de cuando se quiere eliminar un proceso
            registros->PC++;
            /*proceso_actual_ejecutando= false;  para prueba univocas*/ 
        }
        // 
        if (mandar_pcb)
            enviar_pcb(MOTIVO_DESALOJO_INTERRUPCION); //hubo una interrupcion y se debera mandar el pcb a kernel
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
            log_info(logger, "Recibi el mensaje: %s.", mensaje);
            // no deberia haber ningun caso de este estilo
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(socket_cliente_interrupt, logger);
            log_info(logger, "Recibi el id que quiere se quiere eliminar: %d.", list_get(lista, 0));
            // el paquete tiene el id pero no me importa, nomas ahora cambiar interrupcion a true

            // al modificar la variable global, necesito que no se interrumpa en el medio
            pthread_mutex_lock(&mutex_interrupcion);
            interrupcion = true;
            pthread_mutex_unlock(&mutex_interrupcion);
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

e_instruccion parsear_instruccion(char* instruccionString){
    e_instruccion enum_instruccion;

    if (string_equals_ignore_case(instruccionString,"SET")) {
        enum_instruccion = SET;
    }
    else if (string_equals_ignore_case(instruccionString,"MOV_IN")){
        enum_instruccion = MOV_IN;
    }
    else if (string_equals_ignore_case(instruccionString,"MOV_OUT")){
        enum_instruccion = MOV_OUT;
    }
    else if (string_equals_ignore_case(instruccionString,"SUM")){
        enum_instruccion = SUM;
    }
    else if (string_equals_ignore_case(instruccionString,"SUB")) {
        enum_instruccion = SUB;
    }
    else if (string_equals_ignore_case(instruccionString,"JNZ")){
        enum_instruccion = JNZ;
    }
    else if (string_equals_ignore_case(instruccionString,"RESIZE")){
        enum_instruccion = RESIZE;
    }
    else if (string_equals_ignore_case(instruccionString,"COPY_STRING")){
        enum_instruccion = COPY_STRING;
    }
    else if (string_equals_ignore_case(instruccionString,"WAIT")){
        enum_instruccion = WAIT;
    }
    else if (string_equals_ignore_case(instruccionString,"SIGNAL")){
        enum_instruccion = SIGNAL;
    }
    else if (string_equals_ignore_case(instruccionString,"IO_GEN_SLEEP")){
        enum_instruccion = IO_GEN_SLEEP;
    }
    else if (string_equals_ignore_case(instruccionString,"IO_STDIN_READ")){
        enum_instruccion = IO_STDIN_READ;
    }
    else if (string_equals_ignore_case(instruccionString,"IO_STDOUT_WRITE")){
        enum_instruccion = IO_STDOUT_WRITE;
    }
    else if (string_equals_ignore_case(instruccionString,"IO_FS_CREATE")){
        enum_instruccion = IO_FS_CREATE;
    }
    else if (string_equals_ignore_case(instruccionString,"IO_FS_DELETE")){
        enum_instruccion = IO_FS_DELETE;
    }
    else if (string_equals_ignore_case(instruccionString,"IO_FS_TRUNCATE")){
        enum_instruccion = IO_FS_TRUNCATE;
    }
    else if (string_equals_ignore_case(instruccionString,"IO_FS_WRITE")){
        enum_instruccion = IO_FS_WRITE;
    }
    else if (string_equals_ignore_case(instruccionString,"IO_FS_READ")){
        enum_instruccion = IO_FS_READ;
    }
    else if (string_equals_ignore_case(instruccionString,"EXIT")) {
        enum_instruccion = INSTRUCTION_EXIT;    
    }
    return enum_instruccion;
}


void fetch() 
{
    log_trace(logger, "Estoy en el fetch con el PC %d", registros->PC);
    //Buscamos la siguiente instruccion con el pc en memoria y la asignamos a la variable instruccion
    // char instr_recibida = recibir_mensaje(socket, logger); para conseguir la instruccion
    // por ahora lo hacemos default

    t_paquete* envioMemoria = crear_paquete();
    agregar_a_paquete(envioMemoria, registros->PID, sizeof(uint32_t));
    agregar_a_paquete(envioMemoria, registros->PC, sizeof(uint32_t));
    enviar_paquete(envioMemoria,cliente_memoria, logger);

    log_debug(logger, "Se envio el PID %d y el PC %d a memoria", registros->PID, registros->PC);
    recibir_operacion(cliente_memoria, logger);
    linea_de_instruccion = recibir_mensaje(cliente_memoria, logger);
    
    linea_de_instruccion = "SUM AX BX";
    log_debug(logger, "La instruccion leida es %s", linea_de_instruccion);
    eliminar_paquete(envioMemoria);
    return EXIT_SUCCESS;
}

void decode(){
    linea_de_instruccion_separada = string_split(linea_de_instruccion, " ");

    instruccion = parsear_instruccion(linea_de_instruccion_separada[0]);

    return EXIT_SUCCESS;

}

void execute() {
    log_trace(logger, "Estoy en el execute");
    switch (instruccion)
    {
    case SET:
        instruction_set();
        break;
    case MOV_IN:
        break;
    case MOV_OUT:
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
        break;
    case COPY_STRING:
        break;
    case WAIT:
        instruccion_wait();
        break;
    case SIGNAL:
        instruccion_signal();
        break;
    case IO_GEN_SLEEP:
        break;
    case IO_STDIN_READ:
        break;
    case IO_STDOUT_WRITE:
        break;
    case IO_FS_CREATE:
        break;
    case IO_FS_DELETE:
        break;
    case IO_FS_TRUNCATE:
        break;
    case IO_FS_WRITE:
        break;
    case IO_FS_READ:
        break;
    case INSTRUCTION_EXIT:
        instruccion_exit();
        break;
    default:
        log_error(logger, "instruccion incorreta");
        break;
    }
}

void instruction_set(){
    log_trace(logger, "EJECUTANDO LA INSTRUCCION SET");
    char* registroDestino = string_new();
    char* valorEnString = string_new();

    registroDestino = linea_de_instruccion_separada[1];
    log_debug(logger, "Registro Destino: %s", registroDestino);

    valorEnString = linea_de_instruccion_separada[2];
    log_debug(logger, "Valor en string: %s", valorEnString);

    int valorEnInt = atoi(valorEnString);
    log_debug(logger, "Valor en string: %d", valorEnInt);

    // Establecer el valor en el registro correspondiente
    asignarValoresIntEnRegistros(registroDestino, valorEnInt, "SET");

    free(registroDestino);
    free(valorEnString);
    return EXIT_SUCCESS;
}

void instruccion_sum(){
    log_trace(logger, "EJECUTANDO LA INSTRUCCION SUM");

    char* registroDestino = string_new();
    registroDestino = linea_de_instruccion_separada[1];
    
    char* registroOrigen = string_new();
    registroOrigen = linea_de_instruccion_separada[2];

    log_debug(logger, "Registro Destino: %s", registroDestino);
    log_debug(logger, "Registro Origen: %s", registroOrigen);

    // Obtengo el valor almacenado en el registro uint8_t
    int valorDestino = obtenerValorRegistros(registroDestino);
    int valorOrigen = obtenerValorRegistros(registroOrigen); 
    int valorFinal = valorOrigen + valorDestino;

    asignarValoresIntEnRegistros(registroDestino,valorFinal,"SUM");
    free(registroDestino);
    free(registroOrigen);
}

void instruccion_jnz(){
    log_trace(logger, "EJECUTANDO LA INSTRUCCION JNZ");

    char* registroDestino = string_new();
    registroDestino = linea_de_instruccion_separada[0];

    char* parametro = string_new();
    parametro = linea_de_instruccion[1];

    int nuevoPC = atoi(parametro);

    int valorDelRegistro = obtenerValorRegistros(registroDestino);  
    if(valorDelRegistro != 0) {
        registros->PC = (uint32_t) nuevoPC;
        log_debug(logger, "Se actualizo el PC a la instruccion nro: %d", nuevoPC);
        return;
    }

    free(registroDestino);
    free(parametro);
}

void instruccion_sub(){
    log_trace(logger, "EJECUTANDO LA INSTRUCCION SUB");

    char* registroDestino = string_new();
    registroDestino = linea_de_instruccion_separada[1];
    
    char* registroOrigen = string_new();
    registroOrigen = linea_de_instruccion_separada[2];

    // Obtengo el valor almacenado en el registro uint8_t
    int valorOrigen = obtenerValorRegistros(registroOrigen);  
    int valorDestino = obtenerValorRegistros(registroDestino);
    int valorFinal = valorDestino - valorOrigen;

    asignarValoresIntEnRegistros(registroDestino,valorFinal,"SUB");
    free(registroDestino);
    free(registroOrigen);
}

void instruccion_wait(){
    // envio el pcb a kernel y luego un mensaje con el nombre del recurso que quiero
    // Esta instrucción solicita al Kernel que se asigne una instancia del recurso indicado por parámetro.
    log_trace(logger, "EJECUTANDO LA INSTRUCCION WAIT");
    log_trace(logger, "CPU va a enviar un PCB a Kernel");
    char* nombre_recurso= linea_de_instruccion_separada[1];
    registros->motivo_desalojo = MOTIVO_DESALOJO_WAIT;
    t_paquete *paquete_de_pcb = crear_paquete();
    empaquetar_registros(paquete_de_pcb,registros);
    agregar_a_paquete(paquete_de_pcb,nombre_recurso,strlen(nombre_recurso)+1);
    enviar_paquete(paquete_de_pcb,socket_cliente_dispatch,logger);


    char* resultado=recibir_mensaje(socket_cliente_dispatch,logger);
    // Convierte resultado a int
    int resultado_numero=atoi(resultado);
    // En nuestro conversacion con kernel si es 0 consigio el recurso y 1 significa que, o no existe, o no me lo puede dar
    if (resultado_numero == 1){
        log_debug(logger,"No se pudo obtener el recurso %s",nombre_recurso);
        proceso_actual_ejecutando=false;
    }
}

void instruccion_signal(){
    // Esta instrucción solicita al Kernel que se libere una instancia del recurso indicado por parámetro.
    log_trace(logger, "EJECUTANDO LA INSTRUCCION SIGNAL");
    log_trace(logger, "CPU va a enviar un PCB a Kernel");
    char* nombre_recurso= linea_de_instruccion_separada[1];
    registros->motivo_desalojo = MOTIVO_DESALOJO_SIGNAL;
    t_paquete *paquete_de_pcb = crear_paquete();
    empaquetar_registros(paquete_de_pcb,registros);
    agregar_a_paquete(paquete_de_pcb,nombre_recurso,strlen(nombre_recurso)+1);
    enviar_paquete(paquete_de_pcb,socket_cliente_dispatch,logger);
    char* resultado=recibir_mensaje(socket_cliente_dispatch,logger);
    // Convierte resultado a int
    int resultado_numero=atoi(resultado);
    // todavia no se nuestra conversacion con kernel de esto
    if(resultado_numero == 1){
        log_debug(logger,"No existia ese recurso %s",nombre_recurso);
        proceso_actual_ejecutando=false;
    }
}

void instruccion_io_gen_sleep() {
    log_trace(logger, "EJECUTANDO LA INSTRUCCION IO_GEN_SLEEP");

    char* nroInterfaz = string_new();
    nroInterfaz = linea_de_instruccion_separada[1];

    char* tiempoTrabajoString = string_new();
    tiempoTrabajoString = linea_de_instruccion_separada[2];

    int tiempoTrabajo = atoi(tiempoTrabajoString);


    // Creo paquete
    // Envio el PCB a Kernel
    t_paquete* paqueteAKernel = crear_paquete();

    log_trace(logger, "CPU va a enviar un PCB a Kernel");;
    registros->motivo_desalojo = MOTIVO_DESALOJO_IO_GEN_SLEEP;
    empaquetar_registros(paqueteAKernel,registros);
    agregar_a_paquete(paqueteAKernel,nroInterfaz,strlen(nroInterfaz)+1);
    agregar_a_paquete(paqueteAKernel,tiempoTrabajo,sizeof(int));
    enviar_paquete(paqueteAKernel,socket_cliente_dispatch,logger);

    // Espero la respuesta de Kernel
    char* mensajeKernel = string_new();
    mensajeKernel = recibir_mensaje(socket_cliente_dispatch,logger);

    // Si me devuelve 0 esta todo OK, si no todo MAL
    if(strcmp(mensajeKernel, "0") != 0) {
        proceso_actual_ejecutando = false;
    }

    // LIBERAR LA MEMORIA
    free(nroInterfaz);
    free(tiempoTrabajoString);
    eliminar_paquete(mensajeKernel);
    eliminar_paquete(paqueteAKernel);
}

void instruccion_exit(){
    log_trace(logger, "EJECUTANDO LA INSTRUCCION EXIT");
    // deberia mandar el pcb a kernel
    enviar_pcb(MOTIVO_DESALOJO_EXIT);
    proceso_actual_ejecutando = false;
}

void asignarValoresIntEnRegistros(char* registroDestino, int valor, char* instruccion) {

    log_debug(logger, "%d", strcmp(registroDestino, "BX"));

    if (strcmp(registroDestino, "AX") == 0) {
        registros->AX = (uint8_t)valor;
    } else if (strcmp(registroDestino, "BX") == 0) {
        registros->BX = (uint8_t)valor;
    } else if (strcmp(registroDestino, "CX") == 0) {
        registros->CX = (uint8_t)valor;
    } else if (strcmp(registroDestino, "DX") == 0) {
        registros->DX = (uint8_t)valor;
    } else if (strcmp(registroDestino, "EAX") == 0) {
        registros->EAX = (uint32_t)valor;
    } else if (strcmp(registroDestino, "EBX") == 0) {
        registros->EBX = (uint32_t)valor;
    } else if (strcmp(registroDestino, "ECX") == 0) {
        registros->ECX = (uint32_t)valor;
    } else if (strcmp(registroDestino, "EDX") == 0) {
        registros->EDX = (uint32_t)valor;
    } else if (strcmp(registroDestino, "SI") == 0) {
        registros->SI = (uint32_t)valor;
    } else if (strcmp(registroDestino, "DI") == 0) {
        registros->DI = (uint32_t)valor;
    } else {
        log_error(logger, "Hubo un error al ejecutar la instruccion %s", instruccion);
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int obtenerValorRegistros(char* registroCPU){

    log_debug(logger,"%d", (int) registros->BX);

    if (strcmp(registroCPU, "AX") == 0) {
        return (int) registros->AX;
    } else if (strcmp(registroCPU, "BX") == 0) {
        return (int) registros->BX;
    } else if (strcmp(registroCPU, "CX") == 0) {
        return (int) registros->CX;
    } else if (strcmp(registroCPU, "DX") == 0) {
        return (int) registros->DX;
    } else if (strcmp(registroCPU, "EAX") == 0) {
        return (int) registros->EAX;
    } else if (strcmp(registroCPU, "EBX") == 0) {
        return (int) registros->EBX;
    } else if (strcmp(registroCPU, "ECX") == 0) {
        return (int) registros->ECX;
    } else if (strcmp(registroCPU, "EDX") == 0) {
        return (int) registros->EDX;
    } else if (strcmp(registroCPU, "SI") == 0) {
        return (int) registros->SI;
    } else if (strcmp(registroCPU, "DI") == 0) {
        return (int) registros->DI;
    } else {
        log_error(logger, "NO SE PUDO OBTENER EL VALOR DEL REGISTRO: %s", registroCPU);
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

void check_interrupt(){
    pthread_mutex_lock(&mutex_interrupcion);
    if (interrupcion) {
        log_trace(logger, "Se recibio una interrupcion");
        mandar_pcb = true;
        proceso_actual_ejecutando = false;
        interrupcion = false;
    }
    pthread_mutex_unlock(&mutex_interrupcion);
} 

void enviar_pcb(e_motivo_desalojo motivo_desalojo, Agregar_datos_paquete agregar_datos_paquete, void* datos){
    log_trace(logger, "CPU va a enviar un PCB a Kernel");
    registros->motivo_desalojo = motivo_desalojo;
    t_paquete *paquete_de_pcb = crear_paquete();
    empaquetar_registros(paquete_de_pcb,registros);
    Agregar_datos_paquete(paquete_de_pcb,datos);
    enviar_paquete(paquete_de_pcb,socket_cliente_dispatch,logger);
}

void agregar_datos_recurso(t_paquete* paquete, void* nombre_recurso){
    //en este caso datos va a ser el nombre del recurso que voy a pedir o devolver
    agregar_a_paquete(paquete,nombre_recurso,strlen(nombre_recurso)+1);
}

void agregar_datos_tiempo(t_paquete* paquete, void* datos){
    // en este caso datos tiene dos cosas el numero de interfaz y el tiempo de trabajo
    char** datos_tiempo = (char**) datos;
    agregar_a_paquete(paquete,datos_tiempo[0],strlen(datos_tiempo[0])+1);
    agregar_a_paquete(paquete,datos_tiempo[1],sizeof(int));
}

void no_agregar_datos(t_paquete* paquete, void* datos){
    // no se agrega nada
}

