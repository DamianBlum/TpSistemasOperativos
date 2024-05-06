#include "pcb.h"

bool MOCKUP_RESPUESTA_CPU = false;

t_PCB *crear_pcb(int *contadorProcesos, int quantum)
{
    t_PCB *pcb = malloc(sizeof(t_PCB));
    pcb->processID = *contadorProcesos;
    (*contadorProcesos)++;
    pcb->estado = E_NEW;
    pcb->programCounter = 0;
    pcb->quantum = quantum;
    pcb->AX = 0;
    pcb->BX = 0;
    pcb->CX = 0;
    pcb->DX = 0;
    pcb->EAX = 0;
    pcb->EBX = 0;
    pcb->ECX = 0;
    pcb->EDX = 0;
    pcb->SI = 0;
    pcb->DI = 0;
    pcb->recursos_asignados = list_create();
    // pcb->temporal = temporal_create(); el create lo hago en el ready_a_exec ya q arranca el cronometro, desp lo destruyo en el exec_a_algo
    return pcb;
}

void empaquetar_pcb(t_paquete *p, t_PCB *pcb)
{ // yo mando todo el pcb, pero en cpu dudo q usen todo
    agregar_a_paquete(p, pcb->processID, sizeof(uint32_t));
    agregar_a_paquete(p, pcb->programCounter, sizeof(uint32_t));
    agregar_a_paquete(p, pcb->quantum, sizeof(uint32_t));
    agregar_a_paquete(p, (uint8_t)pcb->estado, sizeof(uint8_t));
    agregar_a_paquete(p, pcb->AX, sizeof(uint8_t));
    agregar_a_paquete(p, pcb->BX, sizeof(uint8_t));
    agregar_a_paquete(p, pcb->CX, sizeof(uint8_t));
    agregar_a_paquete(p, pcb->DX, sizeof(uint8_t));
    agregar_a_paquete(p, pcb->EAX, sizeof(uint32_t));
    agregar_a_paquete(p, pcb->EBX, sizeof(uint32_t));
    agregar_a_paquete(p, pcb->ECX, sizeof(uint32_t));
    agregar_a_paquete(p, pcb->EDX, sizeof(uint32_t));
    agregar_a_paquete(p, pcb->SI, sizeof(uint32_t));
    agregar_a_paquete(p, pcb->DI, sizeof(uint32_t));
}

void desempaquetar_pcb_a_registros(t_list *paquetes, t_registros *regs, t_log *logger)
{
    log_debug(logger, "Voy a actualizar los registros con el contexto del proceso %d", (int)list_get(paquetes, 0));
    log_debug(logger, "Valores de los registros antes de ser modificados: PC=%d | AX=%d | BX=%d | CX=%d | DX=%d | EAX=%d | EBX=%d | ECX=%d | EDX=%d | SI=%d | DI=%d", regs->PC, regs->AX, regs->BX, regs->CX, regs->DX, regs->EAX, regs->EBX, regs->ECX, regs->EDX, regs->SI, regs->DI);
    if (MOCKUP_RESPUESTA_CPU) // esto desp vuela
    {                         // es para q puedan avanzar en cpu antes de q tengamos lo de kernel
        regs->PID = (uint32_t)10;
        regs->PC = (uint32_t)0;
        regs->AX = (uint8_t)1;
        regs->BX = (uint8_t)2;
        regs->CX = (uint8_t)3;
        regs->DX = (uint8_t)4;
        regs->EAX = (uint32_t)11;
        regs->EBX = (uint32_t)12;
        regs->ECX = (uint32_t)13;
        regs->EDX = (uint32_t)14;
        regs->SI = (uint32_t)44;
        regs->DI = (uint32_t)45;
    }
    else
    {
        regs->PID = (uint32_t)list_get(paquetes, 0);
        regs->PC = (uint32_t)list_get(paquetes, 1);
        regs->AX = (uint8_t)list_get(paquetes, 4);
        regs->BX = (uint8_t)list_get(paquetes, 5);
        regs->CX = (uint8_t)list_get(paquetes, 6);
        regs->DX = (uint8_t)list_get(paquetes, 7);
        regs->EAX = (uint32_t)list_get(paquetes, 8);
        regs->EBX = (uint32_t)list_get(paquetes, 9);
        regs->ECX = (uint32_t)list_get(paquetes, 10);
        regs->EDX = (uint32_t)list_get(paquetes, 11);
        regs->SI = (uint32_t)list_get(paquetes, 12);
        regs->DI = (uint32_t)list_get(paquetes, 13);
    }
    log_debug(logger, "Valores de los registros ya modificados: PC=%d | AX=%d | BX=%d | CX=%d | DX=%d | EAX=%d | EBX=%d | ECX=%d | EDX=%d | SI=%d | DI=%d", regs->PC, regs->AX, regs->BX, regs->CX, regs->DX, regs->EAX, regs->EBX, regs->ECX, regs->EDX, regs->SI, regs->DI);
}

void actualizar_pcb(t_list *paquetes, t_PCB *pcb, t_log *logger)
{
    log_debug(logger, "\nContenido del pcb antes de actualizarlo:\n|Process id: %d|\n|Program counter: %d|\n|Quantum: %d|\n|Estado: %d|\n|AX: %d|\n|BX: %d|\n|CX: %d|\n|DX: %d|\n|EAX=%d|\n|EBX=%d|\n|ECX=%d|\n|EDX=%d|\n|SI=%d|\n|DI=%d", pcb->processID, pcb->programCounter, pcb->quantum, (int)pcb->estado, pcb->AX, pcb->BX, pcb->CX, pcb->DX, pcb->EAX, pcb->EBX, pcb->ECX, pcb->EDX, pcb->SI, pcb->DI);

    pcb->programCounter = (uint32_t)list_get(paquetes, 1);
    pcb->AX = (uint8_t)list_get(paquetes, 3);
    pcb->BX = (uint8_t)list_get(paquetes, 4);
    pcb->CX = (uint8_t)list_get(paquetes, 5);
    pcb->DX = (uint8_t)list_get(paquetes, 6);
    pcb->EAX = (uint32_t)list_get(paquetes, 7);
    pcb->EBX = (uint32_t)list_get(paquetes, 8);
    pcb->ECX = (uint32_t)list_get(paquetes, 9);
    pcb->EDX = (uint32_t)list_get(paquetes, 10);
    pcb->SI = (uint32_t)list_get(paquetes, 11);
    pcb->DI = (uint32_t)list_get(paquetes, 12);

    log_debug(logger, "\nContenido del pcb despues de actualizarlo:\n|Process id: %d|\n|Program counter: %d|\n|Quantum: %d|\n|Estado: %d|\n|AX: %d|\n|BX: %d|\n|CX: %d|\n|DX: %d|\n|EAX=%d|\n|EBX=%d|\n|ECX=%d|\n|EDX=%d|\n|SI=%d|\n|DI=%d", pcb->processID, pcb->programCounter, pcb->quantum, (int)pcb->estado, pcb->AX, pcb->BX, pcb->CX, pcb->DX, pcb->EAX, pcb->EBX, pcb->ECX, pcb->EDX, pcb->SI, pcb->DI);
}

t_PCB *devolver_pcb_desde_lista(t_list *lista, uint32_t id)
{
    for (int i = 0; i < list_size(lista); i++)
    {
        t_PCB *pcb_aux = (t_PCB *)list_get(lista, i);
        if (pcb_aux->processID == id)
            return pcb_aux;
    }
    return NULL;
}

void destruir_pcb(t_PCB *pcb)
{
    // hacer desp je
}

char *estado_proceso_texto(e_estado_proceso estado)
{
    char *estado_texto;
    switch (estado)
    {
    case E_NEW:
        estado_texto = string_duplicate("NEW");
        break;
    case E_READY:
        estado_texto = string_duplicate("READY");
        break;
    case E_READY_PRIORITARIO:
        estado_texto = string_duplicate("READY PRIORITARIO");
        break;
    case E_RUNNING:
        estado_texto = string_duplicate("EXECUTE");
        break;
    case E_BLOCKED:
        estado_texto = string_duplicate("BLOCKED");
        break;
    case E_EXIT:
        estado_texto = string_duplicate("EXIT");
        break;
    default:
        estado_texto = NULL;
        break;
    }
    return estado_texto;
}