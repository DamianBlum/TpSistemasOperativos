#ifndef PCB_H_
#define PCB_H_

#include <netdb.h>
#include <utils/sockets.h>
#include <utils/registros.h>

typedef enum
{
    NEW = 0,
    READY,
    EXEC,
    BLOCKED,
    EXIT,
} e_estado_proceso;

typedef struct
{
    uint32_t processID;
    uint32_t programCounter;
    uint32_t quantum; // esto no tiene mucho sentido q este aca
    uint32_t registros[CANTIDAD_REGISTROS];
    e_estado_proceso estado;
} t_PCB;

t_PCB *crear_pcb(int *contadorProcesos);

void empaquetar_pcb(t_paquete *p, t_PCB pcb); // mete en el paquete (tiene q estar ya creado), toda la data del pcb

t_PCB *desempaquetar_pcb(t_list *paquetes); // con la lista q tenes cuando recibis un paquete, arma un pcb de vuelta, la idea de esta funcion es q se use en el modulo de CPU cuando recibis un contexto

void actualizar_pcb(t_list *paquetes, t_PCB *pcb); // lo mismo q el de arriba pero en vez de darte un pcb, te actualiza el enviado x param

#endif