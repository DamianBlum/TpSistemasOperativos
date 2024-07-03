#ifndef CPU_H_
#define CPU_H_

#include <math.h>
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
#include <utils/operacionMemoriaUtils.h>

typedef enum
{
    DISPATCH = 0,
    INTERRUPT,
    MMU
} e_hilos_kernel;

typedef enum
{
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

typedef struct
{
    int PID;
    uint32_t nro_pag;
    uint32_t nro_marco;
    uint32_t instante_refencia;
} t_TLB;

typedef void (*Agregar_datos_paquete)(t_paquete *, void *);

int main(int argc, char *argv[]);
void *servidor_dispatch(void *arg);
void *servidor_interrupt(void *arg);
char *componente_mmu(char *registro, uint32_t pid);
int fetch();
void decode();
void execute();
void check_interrupt();
void enviar_pcb(e_motivo_desalojo motivo_desalojo, Agregar_datos_paquete agregar_datos_paquete, void *datos);
void no_agregar_datos(t_paquete *paquete, void *datos);
void agregar_datos_tiempo(t_paquete *paquete, void *datos);
void agregar_datos_recurso(t_paquete *paquete, void *nombre_recurso);
void agregar_datos_interfaz_std(t_paquete *paquete, void *datos);
void agregar_datos_interfaz_create_delete(t_paquete *paquete, void *datos);
void agregar_datos_interfaz_truncate(t_paquete *paquete, void *datos);
void agregar_datos_interfaz_read_write(t_paquete *paquete, void *datos);
#endif