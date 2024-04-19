#ifndef KERNEL_H_
#define KERNEL_H_

#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <utils/configUtils.h>
#include <utils/sockets.h>
#include <utils/pcb.h>
#include <commons/string.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <readline/readline.h>

typedef enum
{
    FIFO = 0,
    RR,
    VRR,
} e_algoritmo_planificacion;

int main(int argc, char *argv[]);
int generar_clientes();
void *atender_servidor_io(void *arg);
void mostrar_menu();
e_algoritmo_planificacion obtener_algoritmo_planificacion(char *algo);
void instanciar_colas();

// planificacion de largo plazo
void crear_proceso();
void eliminar_proceso();

// planificacion de corto plazo

// funciones de manejo de lista_de_pcb
t_PCB *obtener_pcb_de_lista_por_id(int id);

// cambios de estados
void evaluar_NEW_a_READY();
void evaluar_NEW_a_EXIT();
void evaluar_READY_a_EXEC();
void evaluar_EXEC_a_READY();
void evaluar_READY_a_EXIT();
void evaluar_BLOCKED_a_EXIT();
void evaluar_EXEC_a_BLOCKED();
void evaluar_BLOCKED_a_READY();
void evaluar_EXEC_a_EXIT();

#endif