#include "registros.h"

void empaquetar_registros(t_paquete *p, t_registros *t)
{
    agregar_a_paquete(p, t->PID, sizeof(uint32_t));
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
    agregar_a_paquete(p, (uint8_t)t->motivo_desalojo, sizeof(uint8_t));
}

t_registros *crear_registros()
{
    t_registros *r = malloc(sizeof(t_registros));
    r->PID = 0;
    r->PC = 0;
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
    r->motivo_desalojo = MOTIVO_DESALOJO_EXIT;
    return r;
}

t_registros *destruir_registros(t_registros *t)
{
    free(t);
}

e_motivo_desalojo conseguir_motivo_desalojo_de_registros_empaquetados(t_list *lista)
{
    return (e_motivo_desalojo)list_get(lista, 12);
}

char *motivo_desalojo_texto(e_motivo_desalojo e)
{
    char *motivo;
    switch (e)
    {
    case MOTIVO_DESALOJO_EXIT:
        motivo = string_duplicate("EXIT");
        break;
    case MOTIVO_DESALOJO_INTERRUPCION:
        motivo = string_duplicate("INTERRUPCION");
        break;
    case MOTIVO_DESALOJO_WAIT:
        motivo = string_duplicate("WAIT");
        break;
    case MOTIVO_DESALOJO_SIGNAL:
        motivo = string_duplicate("SIGNAL");
        break;
    case MOTIVO_DESALOJO_IO_GEN_SLEEP:
        motivo = string_duplicate("IO SLEEP");
        break;
    case MOTIVO_DESALOJO_IO_STDIN_READ:
        motivo = string_duplicate("IO STDIN READ");
        break;
    case MOTIVO_DESALOJO_IO_STDOUT_WRITE:
        motivo = string_duplicate("IO STDOUT WRITE");
        break;
    case MOTIVO_DESALOJO_IO_FS_CREATE:
        motivo = string_duplicate("IO FS CREATE");
        break;
    case MOTIVO_DESALOJO_IO_FS_DELETE:
        motivo = string_duplicate("IO FS DELETE");
        break;
    case MOTIVO_DESALOJO_IO_FS_TRUNCATE:
        motivo = string_duplicate("IO FS TRUNCATE");
        break;
    case MOTIVO_DESALOJO_IO_FS_WRITE:
        motivo = string_duplicate("IO FS WRITE");
        break;
    case MOTIVO_DESALOJO_IO_FS_READ:
        motivo = string_duplicate("IO FS READ");
        break;
    default:
        motivo = string_duplicate("Chanfles");
        break;
    }
}