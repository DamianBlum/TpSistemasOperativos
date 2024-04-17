#ifndef PCB_H_
#define PCB_H_

#include <netdb.h>
#include <utils/sockets.h>
#include <utils/registros.h>

typedef enum
{
    E_NEW = 0,
    E_READY,
    E_EXECUTE,
    E_BLOCKED,
    E_EXIT,
} e_estado_proceso;

typedef struct
{
    uint32_t processID;
    uint32_t programCounter;
    uint32_t quantum; // esto no tiene mucho sentido q este aca
    e_estado_proceso estado;
    uint8_t AX;
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI;
    uint32_t DI;
} t_PCB;

t_PCB *crear_pcb(int *contadorProcesos, int quantum);

void empaquetar_pcb(t_paquete *p, t_PCB *pcb); // mete en el paquete (tiene q estar ya creado), toda la data del pcb

void desempaquetar_pcb_a_registros(t_list *paquetes, t_registros *regs, t_log *logger); // con la lista q tenes cuando recibis un paquete, arma un registros de vuelta, la idea de esta funcion es q se use en el modulo de CPU cuando recibis un contexto

void actualizar_pcb(t_list *paquetes, t_PCB *pcb, t_log *logger); // lo mismo q el de arriba pero en vez de darte un pcb, te actualiza el enviado x param

t_PCB *devolver_pcb_desde_lista(t_list *lista, uint32_t id);

void destruir_pcb(t_PCB *pcb);

#endif