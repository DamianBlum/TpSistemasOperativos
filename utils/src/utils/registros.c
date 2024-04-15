#include "registros.h"

void empaquetar_registros(t_paquete *p, t_registros *t)
{
    agregar_a_paquete(p, t->PC, sizeof(uint32_t));
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
}