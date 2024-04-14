#include "pcb.h"

t_PCB *crear_pcb(int *contadorProcesos, int quantum)
{
    t_PCB *pcb = malloc(sizeof(t_PCB));
    pcb->processID = contadorProcesos;
    contadorProcesos++;
    pcb->estado = NEW;
    pcb->programCounter = 0;
    pcb->quantum = quantum;
    pcb->registros = [ 0, 0, 0, 0, 0, 0, 0 ]; // es un asco esto, si alguien tiene una mejor forma de instanciarlo bienvenido sea
}

void empaquetar_pcb(t_paquete *p, t_PCB pcb)
{
}

t_PCB *desempaquetar_pcb(t_list *paquetes)
{
}

void actualizar_pcb(t_list *paquetes, t_PCB *pcb)
{
}
