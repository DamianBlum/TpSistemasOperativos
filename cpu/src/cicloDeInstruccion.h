#ifndef CICLO_H_
#define CICLO_H_

#include "main.h"

typedef enum {
    SET = 0,
    MOV_IN,
    MOV_OUT,
    SUM,
    SUB,
    JNZ,
    RESIZE,
    COPY_STRING,
    WAIT,
    SIGNAL,
    IO_GEN_SLEEP,
    IO_STDIN_READ,
    IO_STDOUT_WRITE,
    IO_FS_CREATE,
    IO_FS_DELETE,
    IO_FS_TRUNCATE,
    IO_FS_WRITE,
    IO_FS_READ,
    INSTRUCTION_EXIT
} e_instruccion;

void fetch(char* linea_de_instruccion, uint32_t PC, t_log *logger;);
void decode(char* linea_de_instruccion,e_instruccion* instruccion ,char** linea_de_instruccion_separada, t_log *logger;);
void execute(char** linea_de_instruccion_separada, e_instruccion* instruccion, t_registros *registros, t_log *logger;);
void checkInterrupt(); 

#endif