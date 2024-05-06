#ifndef REGISTROS_H_
#define REGISTROS_H_

#include <netdb.h>
#include <utils/sockets.h>

// enum motivo desalojo
typedef enum
{
    MOTIVO_DESALOJO_EXIT = 0,
    MOTIVO_DESALOJO_INTERRUPCION,
    MOTIVO_DESALOJO_WAIT,
    MOTIVO_DESALOJO_SIGNAL,
    MOTIVO_DESALOJO_IO_GEN_SLEEP,
    MOTIVO_DESALOJO_IO_STDIN_READ,
    MOTIVO_DESALOJO_IO_STDOUT_WRITE,
    MOTIVO_DESALOJO_IO_FS_CREATE,
    MOTIVO_DESALOJO_IO_FS_DELETE,
    MOTIVO_DESALOJO_IO_FS_TRUNCATE,
    MOTIVO_DESALOJO_IO_FS_WRITE,
    MOTIVO_DESALOJO_IO_FS_READ,
} e_motivo_desalojo;

// estructura que se va a usar en el modulo CPU
typedef struct
{
    uint32_t PID;
    uint32_t PC; // program counter
    uint8_t AX;  // los 8 de proposito general
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI;                       // tiene la dir logica de origen desde donde se va a copiar un string
    uint32_t DI;                       // tiene la dir logica de memoria de destino a donde se va a copiar un string
    e_motivo_desalojo motivo_desalojo; // pepo pidio que le devolvamos porque se va el proceso, si por quantum, por interrupcio de el o por que la CPU me pidio eliminar el proceso
} t_registros;

void empaquetar_registros(t_paquete *p, t_registros *t);
t_registros *crear_registros();
t_registros *destruir_registros();
e_motivo_desalojo conseguir_motivo_desalojo_de_registros_empaquetados(t_list *lista);
char *motivo_desalojo_texto(e_motivo_desalojo e);

#endif