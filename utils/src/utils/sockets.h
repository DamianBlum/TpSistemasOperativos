#ifndef SOCKETS_UTILS_H_
#define SOCKETS_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>

typedef enum
{
	MENSAJE,
	PAQUETE,
	EXIT
} op_code;

typedef struct
{
	int size;
	void *stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer *buffer;
} t_paquete;

/** ----- FUNCIONES CLIENTE ----- **/

/**
 * @NAME: crear_conexion
 * @DESC: Se encarga de conectarse a un servidor, devolviendo su socket. Le pasas el config, el nombre de las constantes que tienen la ip y el puerto y un logger.
 *
 * Ejemplo: int conexion = crear_conexion(config, "IP_SERVIDOR", "PUERTO_SERVIDOR", logger);
 *
 **/
int crear_conexion(t_config *config, char *key_ip, char *key_puerto, t_log *logger);

/**
 * @NAME: enviar_mensaje
 * @DESC: Envia a un socket especifico un mensaje. Tiene logs a nivel debug.
 * Aclaracion: no importa quien es el cliente y quien el servidor, ambos se pueden enviar paquetes y mensajes.
 *
 * Ejemplo: enviar_mensaje("esto es un mensaje :D", socket, logger)
 *
 **/
void enviar_mensaje(char *mensaje, int socket_cliente, t_log *logger);

/**
 * @NAME: crear_paquete
 * @DESC: Esta funcion se usa para crear los paquetes con todos sus mallocs. El paquete resultante va a estar vacio, se llena con agregar_a_paquete()
 *
 * Ejemplo: t_paquete *paquete = crear_paquete();
 *
 **/
t_paquete *crear_paquete(void);

/**
 * @NAME: agregar_a_paquete
 * @DESC: Sirve para meter cosas dentro de un paquete (por ahora solo funciona con numeros, hay que probar bien con otros tipos de datos).
 *
 * Ejemplo: agregar_a_paquete(paquete, 44, sizeof(int))
 *
 **/
void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio);

/**
 * @NAME: enviar_paquete
 * @DESC: Envia el paquete por el socket definido en los parametros. Tiene logs a nivel de debug.
 *
 * Ejemplo: enviar_paquete(paquete, socket, logger);
 *
 **/
void enviar_paquete(t_paquete *paquete, int socket_cliente, t_log *logger);

/**
 * @NAME: liberar_conexion
 * @DESC: Hace toda la magia (no es mucha magia) para liberar los recursos de la conexion.
 *
 * Ejemplo: liberar_conexion(socket, logger);
 *
 **/
void liberar_conexion(int socket_cliente, t_log *logger);

/**
 * @NAME: eliminar_paquete
 * @DESC: Hace la magia para que no haya memory leaks con los paquetes. De todas formas, no hace falta usarlo ya que se implementa en las funciones de enviar.
 *
 * Ejemplo: eliminar_paquete(paquete);
 *
 **/
void eliminar_paquete(t_paquete *paquete);

/** ----- FUNCIONES SERVIDOR ----- **/

/**
 * @NAME: recibir_buffer
 * @DESC: Se encarga de sacar el buffer de los paquetes. Igual no lo vas a usar xq se usa en funciones de sockets.
 *
 * Ejemplo: void *buffer = recibir_buffer(&size, socket);
 *
 **/
void *recibir_buffer(int *, int);

/**
 * @NAME: iniciar_servidor
 * @DESC: Se encarga de iniciar el servidor. Le pasas un t_config y el nombre de la variable del mismo que tiene el puerto a abrir, retorna el nro del socket.
 *
 * Ejemplo: int socket_svr = iniciar_servidor(config, "PUERTO_SERVER_KERNEL");
 *
 **/
int iniciar_servidor(t_config *config, char *key_puerto);

/**
 * @NAME: esperar_cliente
 * @DESC: Se encarga de esperar el cliente (bloqueante) y devolverlo si la conexion este bien hecha.
 * Le pasas el socket del servidor, y retorna el socket de un cliente que se conecto. Tambien hace el handshake
 *
 * Ejemplo: int socket_cliente = esperar_cliente(socket_svr, logger);
 *
 **/
int esperar_cliente(int, t_log *);

/**
 * @NAME: recibir_paquete
 * @DESC: Mete en una t_list los valores q estan dentro del paquete. Mucho mas no hay para comentar.
 *
 * Ejemplo: t_list listaDePaquetes = recibir_paquete(socket);
 *
 **/
t_list *recibir_paquete(int, t_log *);

/**
 * @NAME: recibir_mensaje
 * @DESC: Guarda el mensaje recibido en el buffer pasado por parametro.
 *
 * Ejemplo: recibir_mensaje(socket, logger, buffer);
 *
 **/
char *recibir_mensaje(int socket, t_log *logger);

/**
 * @NAME: recibir_operacion
 * @DESC: Esto espera que un paquete llegue al socket (es bloqueante) y devuelve el codigo de operacion del mismo. El resto del paquete queda en el socket
 * asi que es necesario extraerlo con recibir_mensaje o recibir_paquete respectivamente.
 *
 * Ejemplo: int op = recibir_operacion(socket);
 *
 **/
int recibir_operacion(int, t_log *);

/**
 * @NAME: recibir_todo
 * @DESC: Es una implementacion rustica que combina recibir_mensaje con recibir_paquete. Usar bajo tu propia responsabilidad.
 *
 * Ejemplo: int estado = recibir_todo(socket, buffer, lista_valores);
 *
 **/
int recibir_todo(int socket_cliente, char **buffer, t_list *valores);

#define DESCONEXION -1

#endif /* SOCKETS_UTILS_H_ */
