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

// Linea de instruccion que llega de memoria
char *linea_de_instruccion;

// instruccion
e_instruccion instruccion;

// Linea de instruccion separada
char **linea_de_instruccion_separada;



int main(int argc, char *argv[])
{
    registros = crear_registros();
    logger = iniciar_logger("cpu.log", "CPU", argc, argv);
    config = iniciar_config("cpu.config");

    linea_de_instruccion = string_new();   
    linea_de_instruccion_separada = string_array_new();
    
    // Parte cliente, por ahora esta harcodeado, despues agregar
    /* 
    cliente_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
    enviar_mensaje("Hola Memoria soy yo, tu cliente CPU :D", cliente_memoria, logger);
    */
    // PARTE SERVIDOR

    
    pthread_create(&tid[DISPATCH], NULL, servidor_dispatch, NULL);
    pthread_create(&tid[INTERRUPT], NULL, servidor_interrupt, NULL);
    
    
    pthread_join(tid[DISPATCH], NULL);
    pthread_join(tid[INTERRUPT], NULL);


    // liberar_conexion(cliente_memoria, logger); cuando pruebe lo de ser cliente de memoria descomentar esto

    // libero todas las variables que uso
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
        
        int proceso_actual_ejecutando = true;
        while (proceso_actual_ejecutando) {
            registros->AX = 2;
            registros->BX = 3;

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
            proceso_actual_ejecutando= false; // por ahora para probar una sola instruccion;
        }
        
        // proceso de enviar el PCB a kernel
        log_trace(logger, "CPU va a enviar un PCB a Kernel");
        t_paquete *paquete_de_pcb =crear_paquete();
        empaquetar_registros(paquete_de_pcb,registros);
        enviar_paquete(paquete_de_pcb,socket_cliente_dispatch,logger);

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
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(socket_cliente_interrupt, logger);
            log_info(logger, "Recibi un paquete.");
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
    log_trace(logger, "Estoy en elfetch con el PC %d", registros->PC);
    //Buscamos la siguiente instruccion con el pc en memoria y la asignamos a la variable instruccion
    // char instr_recibida = recibir_mensaje(socket, logger); para conseguir la instruccion
    // por ahora lo hacemos default
    linea_de_instruccion = string_duplicate("SUB AX BX");
    log_debug(logger, "LA instruccion leida es %s", linea_de_instruccion);
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
        break;
    case SIGNAL:
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

    // Obtengo el valor almacenado en el registro uint8_t
    int valorOrigen = obtenerValorRegistros(registroOrigen);  
    int valorDestino = obtenerValorRegistros(registroDestino);
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

void asignarValoresIntEnRegistros(char* registroDestino, int valor, char* instruccion) {

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

void check_interrupt(){} 
