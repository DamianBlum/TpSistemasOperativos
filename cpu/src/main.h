#ifndef CPU_H_
#define CPU_H_

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <utils/configUtils.h>
#include <commons/string.h>
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <utils/sockets.h>
#include <utils/pcb.h>
#include <utils/registros.h>

typedef enum
{
    DISPATCH = 0,
    INTERRUPT
} e_conexiones_kernel;

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


int main(int argc, char *argv[]);
void *servidor_dispatch(void *arg);
void *servidor_interrupt(void *arg);

void fetch(char* linea_de_instruccion, uint32_t PC, t_log *logger;);
void decode(char* linea_de_instruccion,e_instruccion* instruccion ,char** linea_de_instruccion_separada, t_log *logger;);
void execute(char** linea_de_instruccion_separada, e_instruccion* instruccion, t_registros *registros, t_log *logger;);
void check_interrupt(); 

#endif