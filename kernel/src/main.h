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
#include <math.h> // lo uso para redondear los tiempos de ejecucion
#include <utils/interfazUtils.h>
#include <commons/memory.h>
#include <utils/operacionMemoriaUtils.h>
#include <sys/stat.h>

typedef enum
{
    RECURSO = 0,
    INTERFAZ
} e_tipo_bloqueado;

typedef struct
{
    t_queue *cola_bloqueados;

    // t_manejo_recursos* instancias_recursos; // si es de un recurso
    // t_entrada_salida *interfaz; // si es de una interfaz
    void *datos_bloqueados;
    e_tipo_bloqueado identificador;
} t_manejo_bloqueados;

typedef struct
{
    uint32_t pid;
    t_list *datos;
    e_tipo_interfaz tipo_parametros_io;
} t_pid_con_datos; // esto va en cola de bloqueados

typedef enum
{
    FIFO = 0,
    RR,
    VRR,
} e_algoritmo_planificacion;

typedef struct
{
    char *nombre_interfaz;
    int cliente;
    pthread_mutex_t mutex;   // mutex para el manejo de la interfaz de forma segura
    pthread_mutex_t binario; // esto lo uso para sincronizar
} t_entrada_salida;
typedef struct
{
    uint32_t tamanio;
    uint32_t df;
} t_io_stdin;

typedef struct
{
    uint32_t tamanio;
    uint32_t df;
} t_io_stdout;
typedef struct
{
    t_list *lista_procesos_usando_recurso;
    int instancias_recursos;
} t_manejo_recursos;

int main(int argc, char *argv[]);
uint8_t ejecutar_comando(char *comando);
int generar_clientes();
void *atender_servidor_io(void *arg);
void *atender_cliente_io(void *arg);
void *atender_respuesta_proceso(void *arg);
void mostrar_menu();
e_algoritmo_planificacion obtener_algoritmo_planificacion(char *algo);
void instanciar_colas();
void obtener_valores_de_recursos();
uint8_t asignar_recurso(char *recurso, t_PCB *pcb);
uint8_t desasignar_recurso(char *recurso, t_PCB *pcb);
bool eliminar_id_de_la_cola(t_queue *cola, uint32_t id);
bool eliminar_id_de_la_cola_blocked(t_queue *cola, uint32_t id);
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
void evaluar_EXEC_a_BLOCKED(char *recurso, t_list *datos);
void evaluar_BLOCKED_a_READY(t_manejo_bloqueados *tmb);
void evaluar_EXEC_a_EXIT();

void cambiar_estado_proceso(uint32_t id, e_estado_proceso estadoActual, e_estado_proceso nuevoEstado);

void liberar_recursos(t_PCB *pcb); // esto es para el algo_a_EXIT, le saco todos los recursos q tiene asignado el proceso :D

// memoria
void liberar_memoria(uint32_t id);
bool crear_proceso_en_memoria(uint32_t id, char *path);

void *trigger_interrupcion_quantum(void *args);

void cosas_vrr_cuando_se_desaloja_un_proceso(t_PCB *pcb);
bool debe_ir_a_cola_prioritaria(t_PCB *pcb);

void asd(t_manejo_bloqueados *tmb);

t_entrada_salida *obtener_entrada_salida(char *nombre_interfaz);
void crear_interfaz(char *nombre_interfaz, int cliente);
void wait_interfaz(t_entrada_salida *tes);
void signal_interfaz(t_entrada_salida *tes);
t_manejo_bloqueados* conseguir_tmb(uint32_t id);
void sacarle_sus_recursos(uint32_t pid);
bool eliminar_id_lista(t_list* lista, uint32_t id);
#endif