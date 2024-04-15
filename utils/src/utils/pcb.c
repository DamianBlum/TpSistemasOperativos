#include "pcb.h"

bool MOCKUP_RESPUESTA_CPU = true;

t_PCB *crear_pcb(int *contadorProcesos, int quantum)
{
    t_PCB *pcb = malloc(sizeof(t_PCB));
    pcb->processID = *contadorProcesos;
    contadorProcesos++;
    pcb->estado = E_NEW;
    pcb->programCounter = 0;
    pcb->quantum = quantum;
    // pcb->registros = [ 0, 0, 0, 0, 0, 0, 0 ]; // es un asco esto, si alguien tiene una mejor forma de instanciarlo bienvenido sea
    return pcb;
}

void empaquetar_pcb(t_paquete *p, t_PCB pcb)
{
    agregar_a_paquete(p, pcb.processID, sizeof(uint32_t));
    agregar_a_paquete(p, pcb.programCounter, sizeof(uint32_t));
    agregar_a_paquete(p, pcb.quantum, sizeof(uint32_t));
    agregar_a_paquete(p, (uint32_t)pcb.estado, sizeof(uint32_t));
    agregar_a_paquete(p, pcb.registros[SS], sizeof(uint32_t));
    agregar_a_paquete(p, pcb.registros[DS], sizeof(uint32_t));
    agregar_a_paquete(p, pcb.registros[CS], sizeof(uint32_t));
    agregar_a_paquete(p, pcb.registros[AX], sizeof(uint32_t));
    agregar_a_paquete(p, pcb.registros[BX], sizeof(uint32_t));
    agregar_a_paquete(p, pcb.registros[CX], sizeof(uint32_t));
    agregar_a_paquete(p, pcb.registros[DX], sizeof(uint32_t));
}

t_PCB *desempaquetar_pcb(t_list *paquetes, t_log *logger)
{
    t_PCB *pcb;
    if (MOCKUP_RESPUESTA_CPU)
    { // esto es para q puedan avanzar en cpu antes de q tengamos lo de kernel
        int empanada = 0;
        pcb = crear_pcb(&empanada, 44);
        pcb->estado = E_EXECUTE;
        pcb->registros[SS] = 1;
        pcb->registros[DS] = 2;
        pcb->registros[CS] = 3;
        pcb->registros[AX] = 4;
        pcb->registros[BX] = 5;
        pcb->registros[CX] = 6;
        pcb->registros[DX] = 7;
    }
    else
    {
        pcb = crear_pcb((int)list_get(paquetes, 0), list_get(paquetes, 2));
        pcb->programCounter = (uint32_t)list_get(paquetes, 1);
        pcb->estado = (e_estado_proceso)list_get(paquetes, 3);
        pcb->registros[SS] = (uint32_t)list_get(paquetes, 4);
        pcb->registros[DS] = (uint32_t)list_get(paquetes, 5);
        pcb->registros[CS] = (uint32_t)list_get(paquetes, 6);
        pcb->registros[AX] = (uint32_t)list_get(paquetes, 7);
        pcb->registros[BX] = (uint32_t)list_get(paquetes, 8);
        pcb->registros[CX] = (uint32_t)list_get(paquetes, 9);
        pcb->registros[DX] = (uint32_t)list_get(paquetes, 10);
    }
    log_debug(logger, "\nContenido del pcb desempaquetado:\n|Process id: %d|\n|Program counter: %d|\n|Quantum: %d|\n|Estado: %d|\n|SS: %d|\n|DS: %d|\n|CS: %d|\n|AX: %d|\n|BX: %d|\n|CX: %d|\n|DX: %d|\n", pcb->processID, pcb->programCounter, pcb->quantum, (int)pcb->estado, pcb->registros[SS], pcb->registros[DS], pcb->registros[CS], pcb->registros[AX], pcb->registros[BX], pcb->registros[CX], pcb->registros[DX]);

    return pcb;
}

void actualizar_pcb(t_list *paquetes, t_PCB *pcb)
{
}
