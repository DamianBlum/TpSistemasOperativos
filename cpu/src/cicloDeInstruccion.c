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
    *instruccion = "SET AX 1";
    return EXIT_SUCCESS;
}

void decode(char* linea_de_instruccion, e_instruccion instruccion ,char** linea_de_instruccion_separada, t_log *logger){

    char **instr_spliteado = string_array_new();
    linea_de_instruccion_separada = string_split(linea_de_instruccion, " ");
    instruccion = parsear_instruccion(linea_de_instruccion_separada[0]);
    return EXIT_SUCCESS;
    //chararg1 = instr_spliteado[1]; para acceder a algun elemento
}

void execute(char** linea_de_instruccion_separada, e_instruccion instruccion, t_registros *registros, t_log *logger) {
    switch (instruccion)
    {
    case SET:
        instruction_set(linea_de_instruccion_separada, registros);
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

void instruction_set(char** linea_de_instruccion_separada, t_registros* registros){

    char* registroDestino = linea_de_instruccion_separada[1];

    char* valorEnString = linea_de_instruccion_separada[2];

    int valorEnInt = atoi(valorEnString);

    // Establecer el valor en el registro correspondiente
    asignarValoresIntEnRegistros(registros, valorEnInt, "SET");
}

void asignarValoresIntEnRegistros(t_registros* registros, int valor, char* instruccion) {

    if (strcmp(registros, "AX") == 0) {
        registros->AX = (uint8_t)valor;
    } else if (strcmp(registros, "BX") == 0) {
        registros->BX = (uint8_t)valor;
    } else if (strcmp(registros, "CX") == 0) {
        registros->CX = (uint8_t)valor;
    } else if (strcmp(registros, "DX") == 0) {
        registros->DX = (uint8_t)valor;
    } else if (strcmp(registros, "EAX") == 0) {
        registros->EAX = (uint32_t)valor;
    } else if (strcmp(registros, "EBX") == 0) {
        registros->EBX = (uint32_t)valor;
    } else if (strcmp(registros, "ECX") == 0) {
        registros->ECX = (uint32_t)valor;
    } else if (strcmp(registros, "EDX") == 0) {
        registros->EDX = (uint32_t)valor;
    } else if (strcmp(registros, "SI") == 0) {
        registros->SI = (uint32_t)valor;
    } else if (strcmp(registros, "DI") == 0) {
        registros->DI = (uint32_t)valor;
    } else {
        printf("Hubo un error al ejecutar la instruccion %s", instruccion);
        exit(EXIT_FAILURE);
    }
}