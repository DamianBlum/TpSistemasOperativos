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
#include <commons/temporal.h>

typedef struct
{
    t_queue *cola_bloqueados;
    int instancias_recursos;
} t_manejo_bloqueados;

typedef enum
{
    FIFO = 0,
    RR,
    VRR,
} e_algoritmo_planificacion;

int main(int argc, char *argv[]);
int generar_clientes();
void *atender_servidor_io(void *arg);
void *atender_respuesta_proceso(void *arg);
void mostrar_menu();
e_algoritmo_planificacion obtener_algoritmo_planificacion(char *algo);
void instanciar_colas();
void obtener_valores_de_recursos();
uint8_t asignar_recurso(char *recurso, t_PCB *pcb);
uint8_t desasignar_recurso(char *recurso, t_PCB *pcb);
bool eliminar_id_de_la_cola(t_queue *cola, uint32_t id);
t_manejo_bloqueados *crear_manejo_bloqueados();
void destruir_manejor_bloqueados(t_manejo_bloqueados *tmb);
bool pidio_el_recurso(t_PCB *pcb, char *recurso);

// planificacion de largo plazo
void crear_proceso();
void eliminar_proceso();

// planificacion de corto plazo

// funciones de manejo de lista_de_pcb
t_PCB *obtener_pcb_de_lista_por_id(int id);

// cambios de estados
void evaluar_NEW_a_READY();
void evaluar_NEW_a_EXIT(t_PCB *pcb);
void evaluar_READY_a_EXEC();
void evaluar_EXEC_a_READY();
void evaluar_READY_a_EXIT(t_PCB *pcb);
void evaluar_BLOCKED_a_EXIT(t_PCB *pcb);
void evaluar_EXEC_a_BLOCKED(char *recurso);
void evaluar_BLOCKED_a_READY(t_queue *colaRecurso);
void evaluar_EXEC_a_EXIT();

void cambiar_estado_proceso(uint32_t id, e_estado_proceso estadoActual, e_estado_proceso nuevoEstado);

void liberar_recursos(t_PCB *pcb); // esto es para el algo_a_EXIT, le saco todos los recursos q tiene asignado el proceso :D

// memoria
void liberar_memoria(uint32_t id);
void crear_proceso_en_memoria(uint32_t id, char *path);

void *trigger_interrupcion_quantum(void *args);
#endif