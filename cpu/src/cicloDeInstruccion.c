#include "cicloDeInstruccion.h"

e_instruccion parsear_instruccion(char* instruccion){
    e_instruccion enum_instruccion;

    if (string_equals_ignore_case(instruccion,"SET")) {
        enum_instruccion = SET;
    }
    else if (string_equals_ignore_case(instruccion,"MOV_IN")){
        enum_instruccion = MOV_IN;
    }
    else if (string_equals_ignore_case(instruccion,"MOV_OUT")){
        enum_instruccion = MOV_OUT;
    }
    else if (string_equals_ignore_case(instruccion,"SUM")){
        enum_instruccion = SUM;
    }
    else if (string_equals_ignore_case(instruccion,"SUB")) {
        enum_instruccion = SUB;
    }
    else if (string_equals_ignore_case(instruccion,"JNZ")){
        enum_instruccion = JNZ;
    }
    else if (string_equals_ignore_case(instruccion,"RESIZE")){
        enum_instruccion = RESIZE;
    }
    else if (string_equals_ignore_case(instruccion,"COPY_STRING")){
        enum_instruccion = COPY_STRING;
    }
    else if (string_equals_ignore_case(instruccion,"WAIT")){
        enum_instruccion = WAIT;
    }
    else if (string_equals_ignore_case(instruccion,"SIGNAL")){
        enum_instruccion = SIGNAL;
    }
    else if (string_equals_ignore_case(instruccion,"IO_GEN_SLEEP")){
        enum_instruccion = IO_GEN_SLEEP;
    }
    else if (string_equals_ignore_case(instruccion,"IO_STDIN_READ")){
        enum_instruccion = IO_STDIN_READ;
    }
    else if (string_equals_ignore_case(instruccion,"IO_STDOUT_WRITE")){
        enum_instruccion = IO_STDOUT_WRITE;
    }
    else if (string_equals_ignore_case(instruccion,"IO_FS_CREATE")){
        enum_instruccion = IO_FS_CREATE;
    }
    else if (string_equals_ignore_case(instruccion,"IO_FS_DELETE")){
        enum_instruccion = IO_FS_DELETE;
    }
    else if (string_equals_ignore_case(instruccion,"IO_FS_TRUNCATE")){
        enum_instruccion = IO_FS_TRUNCATE;
    }
    else if (string_equals_ignore_case(instruccion,"IO_FS_WRITE")){
        enum_instruccion = IO_FS_WRITE;
    }
    else if (string_equals_ignore_case(instruccion,"IO_FS_READ")){
        enum_instruccion = IO_FS_READ;
    }
    else if (string_equals_ignore_case(instruccion,"EXIT")) {
        enum_instruccion = INSTRUCTION_EXIT;    
    }
    return enum_instruccion;
}


void fetch(char* instruccion, uint32_t pc, t_log *logger) 
{
    //Buscamos la siguiente instruccion con el pc en memoria y la asignamos a la variable instruccion
    // char instr_recibida = recibir_mensaje(socket, logger); para conseguir la instruccion
    // por ahora lo hacemos default
    instruccion = string_duplicate("SET AX 10");
    log_debug(logger, "LA instruccion leida es %s", instruccion);
    return EXIT_SUCCESS;
}

void decode(char* linea_de_instruccion, e_instruccion instruccion ,char** linea_de_instruccion_separada, t_log *logger){

    log_debug(logger, linea_de_instruccion);
    linea_de_instruccion_separada = string_split(linea_de_instruccion, " " );

    log_debug(logger, linea_de_instruccion_separada[0]);
    log_debug(logger, linea_de_instruccion_separada[1]);
    log_debug(logger, linea_de_instruccion_separada[2]);

    instruccion = parsear_instruccion(linea_de_instruccion_separada[0]);
    log_debug(logger, "Instruccion: %s", linea_de_instruccion_separada[0]);
    log_debug(logger, "Registro: %s", linea_de_instruccion_separada[1]);
    log_debug(logger, "Valor: %s", linea_de_instruccion_separada[2]);

    return EXIT_SUCCESS;
    // ["SET","BX","10"]
    //chararg1 = instr_spliteado[1]; para acceder a algun elemento
}

void execute(char** linea_de_instruccion_separada, e_instruccion instruccion, t_registros *registros, t_log *logger) {
    switch (instruccion)
    {
    case SET:
        instruction_set(linea_de_instruccion_separada, registros, logger);
        break;
    case MOV_IN:
        break;
    case MOV_OUT:
        break;
    case SUM:
        break;
    case SUB:
        break;
    case JNZ:
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

void instruction_set(char** linea_de_instruccion_separada, t_registros* registros, t_log* logger){

    char* registroDestino = string_new();
    char* valorEnString = string_new();
    registroDestino = linea_de_instruccion_separada[1];
    log_debug("Registro Destino: %s", registroDestino);

    valorEnString = linea_de_instruccion_separada[2];
    log_debug("Valor en string: %s", valorEnString);

    int valorEnInt = atoi(valorEnString);
    log_debug("Valor en string: %d", valorEnInt);

    // Establecer el valor en el registro correspondiente
    asignarValoresIntEnRegistros(registros, registroDestino,valorEnInt, "SET", logger);

    free(registroDestino);
    free(valorEnString);
}

void asignarValoresIntEnRegistros(t_registros* registros, char* registroDestino,int valor, char* instruccion, t_log* logger) {

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
}