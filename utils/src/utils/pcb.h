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
    uint32_t registros[CANTIDAD_REGISTROS];
} t_PCB;

t_PCB *crear_pcb(int *contadorProcesos, int quantum);

void empaquetar_pcb(t_paquete *p, t_PCB *pcb); // mete en el paquete (tiene q estar ya creado), toda la data del pcb

t_PCB *desempaquetar_pcb(t_list *paquetes, t_log *logger); // con la lista q tenes cuando recibis un paquete, arma un pcb de vuelta, la idea de esta funcion es q se use en el modulo de CPU cuando recibis un contexto

void actualizar_pcb(t_list *paquetes, t_PCB *pcb, t_log *logger); // lo mismo q el de arriba pero en vez de darte un pcb, te actualiza el enviado x param

void destruir_pcb(t_PCB *pcb);

#endif