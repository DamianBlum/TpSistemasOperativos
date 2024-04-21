#ifndef REGISTROS_H_
#define REGISTROS_H_

#include <netdb.h>
#include <utils/sockets.h>

// estructura que se va a usar en el modulo CPU
typedef struct
{
    uint32_t PID;
    uint32_t PC; // program counter
    uint32_t quantum; // para el VRR
    uint8_t AX;  // los 8 de proposito general
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI; // tiene la dir logica de origen desde donde se va a copiar un string
    uint32_t DI; // tiene la dir logica de memoria de destino a donde se va a copiar un string
    uint8_t motivo_interrupcion; //pepo pidio que le devolvamos porque se va el proceso, si por quantum, por interrupcio de el o por que la CPU me pidio eliminar el proceso
} t_registros;

void empaquetar_registros(t_paquete *p, t_registros *t);
t_registros *crear_registros();
t_registros *destruir_registros();

#endif