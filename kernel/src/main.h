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
void mostrar_menu(t_log logger);
e_algoritmo_planificacion obtener_algoritmo_planificacion(char *algo);
void instanciar_colas();

// planificacion de largo plazo
void crear_proceso();
void eliminar_proceso();
void evaluar_NEW_a_READY();
// planificacion de corto plazo

#endif