#include "registros.h"

void empaquetar_registros(t_paquete *p, t_registros *t)
{
    agregar_a_paquete(p, t->PID, sizeof(uint32_t));
    agregar_a_paquete(p, t->PC, sizeof(uint32_t));
    agregar_a_paquete(p, t->quantum, sizeof(uint32_t));
    agregar_a_paquete(p, t->AX, sizeof(uint8_t));
    agregar_a_paquete(p, t->BX, sizeof(uint8_t));
    agregar_a_paquete(p, t->CX, sizeof(uint8_t));
    agregar_a_paquete(p, t->DX, sizeof(uint8_t));
    agregar_a_paquete(p, t->EAX, sizeof(uint32_t));
    agregar_a_paquete(p, t->EBX, sizeof(uint32_t));
    agregar_a_paquete(p, t->ECX, sizeof(uint32_t));
    agregar_a_paquete(p, t->EDX, sizeof(uint32_t));
    agregar_a_paquete(p, t->SI, sizeof(uint32_t));
    agregar_a_paquete(p, t->DI, sizeof(uint32_t));
    agregar_a_paquete(p, t->motivo_interrupcion, sizeof(uint32_t));
}

t_registros *crear_registros()
{
    t_registros *r = malloc(sizeof(t_registros));
    r->PID= 0;
    r->PC = 0;
    r->quantum=0;
    r->AX = 0;
    r->BX = 0;
    r->CX = 0;
    r->DX = 0;
    r->EAX = 0;
    r->EBX = 0;
    r->ECX = 0;
    r->EDX = 0;
    r->SI = 0;
    r->DI = 0;
    r->motivo_interrupcion = 0;
    return r;
}

t_registros *destruir_registros(t_registros *t) {
    free(t);
}