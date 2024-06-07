#include "sockets.h"

/**
 * Client
 */

void *serializar_paquete(t_paquete *paquete, int bytes, t_log *logger)
{
	void *magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento += sizeof(int);
	// log_debug(logger, "DESPLAZAMIENTO 1: %d", desplazamiento);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento += sizeof(int);
	// log_debug(logger, "DESPLAZAMIENTO 2: %d", desplazamiento);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento += paquete->buffer->size;
	// log_debug(logger, "DESPLAZAMIENTO FINAL: %d", desplazamiento);
	return magic;
}

int crear_conexion(t_config *config, char *key_ip, char *key_puerto, t_log *logger)
{
	char *ip = config_get_string_value(config, key_ip);
	char *puerto = config_get_string_value(config, key_puerto);

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &servinfo);

	int sc = socket(servinfo->ai_family,
					servinfo->ai_socktype,
					servinfo->ai_protocol);

	log_info(logger, "Cliente %d creado con exito.", sc);
	log_info(logger, "Cliente %d: Voy a iniciar la conexion con el servidor en %s:%s.", sc, ip, puerto);
	int connection = connect(sc, servinfo->ai_addr, servinfo->ai_addrlen);
	if (connection == -1)
	{
		log_error(logger, "Cliente %d: Fallo la conexion con el servidor %s:%s, se cierra el cliente.", sc, ip, puerto);
		return -1;
	}
	log_info(logger, "Cliente %d: Pude establecer conexion con el servidor %s:%s.", sc, ip, puerto);
	freeaddrinfo(servinfo);

	// handshake
	uint32_t handshake = 1;
	uint32_t result;
	log_info(logger, "Cliente %d: Voy a hacer el handshake.", sc);
	send(sc, &handshake, sizeof(uint32_t), NULL);
	recv(sc, &result, sizeof(uint32_t), MSG_WAITALL);
	if (result == -1)
	{
		log_error(logger, "Cliente %d: Error al hacer el handshake con el servidor. Se cierra la conexion", sc);
		return -1;
	}

	log_info(logger, "Cliente %d: handshake exitoso con el servidor.", sc);
	return sc;
}

void enviar_mensaje(char *mensaje, int socket_cliente, t_log *logger)
{
	log_info(logger, "Mediante el socket %d, se va a enviar el mensaje \"%s\".", socket_cliente, mensaje);
	t_paquete *paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2 * sizeof(int);
	// log_debug(logger, "Size del mensaje a enviar: %d.", bytes);
	void *a_enviar = serializar_paquete(paquete, bytes, logger);

	int c = send(socket_cliente, a_enviar, bytes, 0);
	// log_debug(logger, "Bytes enviados en el mensaje: %d.", c);
	free(a_enviar);
	eliminar_paquete(paquete);
}

void crear_buffer(t_paquete *paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete *crear_paquete(void)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	if (tamanio != sizeof(uint32_t) && tamanio != sizeof(uint8_t))
		memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);
	else // si es un numero tengo meter el &
		memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), &valor, tamanio);
	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete *paquete, int socket_cliente, t_log *logger)
{
	// log_debug(logger, "Se va a enviar un paquete por el socket %d", socket_cliente);
	int bytes = paquete->buffer->size + 2 * sizeof(int);
	void *a_enviar = serializar_paquete(paquete, bytes, logger);
	// log_debug(logger, "Buffer antes de enviar: %d", paquete->buffer->size);
	// log_debug(logger, "Bytes antes de enviar: %d", bytes);
	int c = send(socket_cliente, a_enviar, bytes, 0);
	// log_debug(logger, "Bytes enviados: %d", c);
	free(a_enviar);
	eliminar_paquete(paquete);
}

void eliminar_paquete(t_paquete *paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente, t_log *logger)
{
	log_debug(logger, "Cliente %d: Voy a mandarle al servidor el mensaje de EXIT.", socket_cliente);
	// le mando al svr el mensaje de q se va a cerrar la conexion
	int exit = EXIT; // hago esto xq como debe recibir un puntero rompe las bolas si le pongo solo el EXIT
	send(socket_cliente, &exit, sizeof(int), 0);
	log_debug(logger, "Cliente %d: Mensaje enviado, proceso a cerrar el socket.", socket_cliente);
	// la cierro de este lado
	close(socket_cliente);
}

/**
 * Server
 */

int iniciar_servidor(t_config *config, char *key_puerto)
{
	int socket_servidor;
	char *ip = config_get_string_value(config, "IP");
	char *puerto = config_get_string_value(config, key_puerto);

	// init address info
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &servinfo);

	// init socket
	socket_servidor = socket(servinfo->ai_family,
							 servinfo->ai_socktype,
							 servinfo->ai_protocol);

	if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) 
    	error("setsockopt(SO_REUSEADDR) failed");

	// bind socket to the server's addrinfo
	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

	// starts listening socket
	listen(socket_servidor, SOMAXCONN);

	// frees addrinfo mem space
	freeaddrinfo(servinfo);

	return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log *logger)
{
	uint32_t handshake = 0;
	int socket_cliente;

	while (handshake != 1) // si el handshake falla me quedo esperando a que llegue otro cliente
	{
		// Aceptamos un nuevo cliente
		socket_cliente = accept(socket_servidor, NULL, NULL);
		log_info(logger, "Servidor %d: Se conecto un nuevo cliente, %d.", socket_servidor, socket_cliente);

		// handshake
		uint32_t resultOk = 0;
		uint32_t resultError = -1;

		log_info(logger, "Servidor %d: Esperando el handshake del cliente %d.", socket_servidor, socket_cliente);

		recv(socket_cliente, &handshake, sizeof(uint32_t), MSG_WAITALL);
		if (handshake == 1)
		{
			log_info(logger, "Servidor %d: Handshake exitoso con el cliente %d.", socket_servidor, socket_cliente);
			send(socket_cliente, &resultOk, sizeof(uint32_t), NULL);
		}
		else
		{
			log_error(logger, "Servidor %d: Error al querer hacer el handshake con el cliente %d (Mensaje recibido: \"%d\"). Se va a cerrar la conexion.", socket_servidor, handshake, socket_cliente);
			send(socket_cliente, &resultError, sizeof(uint32_t), NULL);
			close(socket_cliente);
		}
	}

	return socket_cliente;
}

int recibir_operacion(int socket_cliente, t_log *logger)
{
	int cod_op;
	int estado = recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL);
	if (estado > 0)
	{
		log_info("Codigo de operacion recibido: %d | Cantidad de bytes recibidos: %d", cod_op, estado);
		int size;
		// esto no tengo idea xq esta aca, pero bueno, por las dudas lo comento xq tecnicamente no deberia estar
		// int asd = recv(socket_cliente, &size, sizeof(int), MSG_WAITALL);
		// log_info(logger, "SIZE DEL SIZE: %d | SIZE: %d", asd, size);
		return cod_op;
	}
	log_error(logger, "Error al recibir el codigo de operacion.");
	close(socket_cliente);
	return -1;
}

void *recibir_buffer(int *size, int socket_cliente)
{
	void *buffer;
	int asd = recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

char *recibir_mensaje(int socket_cliente, t_log *logger)
{
	int size;
	char *buffer = (char *)recibir_buffer(&size, socket_cliente);
	log_debug(logger, "Mediante el cliente %d, me llego el mensaje \"%s\".", socket_cliente, buffer);
	return buffer;
}

t_list *recibir_paquete(int socket_cliente, t_log *logger)
{
	int size, tamanio;
	int desplazamiento = 0;
	void *buffer;
	t_list *valores = list_create();

	buffer = recibir_buffer(&size, socket_cliente);
	// log_debug(logger, "Size del paquete: %d", size);
	while (desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		// log_debug(logger, "Tamanio obtenido: %d.", tamanio);
		desplazamiento += sizeof(int);
		if (tamanio == sizeof(uint32_t))
		{
			uint32_t valor;
			memcpy(&valor, buffer + desplazamiento, tamanio);
			// log_debug(logger, "Valor uint32 obtenido: %d.", (int)valor);
			desplazamiento += tamanio;
			list_add(valores, valor);
		}
		else if (tamanio == sizeof(uint8_t))
		{
			uint8_t valor;
			memcpy(&valor, buffer + desplazamiento, tamanio);
			// log_debug(logger, "Valor uint8 obtenido: %d.", (int)valor);
			desplazamiento += tamanio;
			list_add(valores, valor);
		}
		else
		{
			char *valor = malloc(tamanio);
			memcpy(valor, buffer + desplazamiento, tamanio);
			// log_debug(logger, "Valor string obtenido: %s.", valor);
			desplazamiento += tamanio;
			list_add(valores, valor);
		}
	}
	free(buffer);
	return valores;
}

int recibir_todo(int socket_cliente, char **buffer, t_list *valores)
{
	int cod_op;
	int desplazamiento = recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL);
	if (cod_op != MENSAJE && cod_op != PAQUETE)
	{
		printf("Error al recibir el paquete, codigo operacion: %d\n", cod_op);
		return 0;
	}
	printf("CODIGO DE OPERACION: %d | CANT BYTES RECV: %d \n", cod_op, desplazamiento);
	int size;
	desplazamiento += recv(socket_cliente, &size, sizeof(int), MSG_WAITALL);
	printf("SIZE DEL SIZE: %d | SIZE: %d\n", desplazamiento, size);
	void *aux = malloc(size);
	desplazamiento += recv(socket_cliente, aux, size, MSG_WAITALL);
	printf("SIZE DEL BUFFER: %d | MSJ: %s\n", desplazamiento, (char *)aux);

	if (cod_op == PAQUETE)
	{
		printf("ENTRE AL COSO DEL PAQUETE! |%d\n", desplazamiento);
		int tamanio;
		desplazamiento = 0;
		while (desplazamiento < size)
		{
			memcpy(&tamanio, aux + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int); // consigo el size del paquetito
			int valor;
			memcpy(&valor, aux + desplazamiento, tamanio); // consigo el buffer del paquetito
			desplazamiento += tamanio;
			list_add(valores, valor);
			printf("VALOOOR: %d\n", valor);
		}
		printf("SIZE LISTA: %d\n", list_size(valores));
	}
	else
	{
		*buffer = string_duplicate((char *)aux);
	}
	free(aux);
	return desplazamiento;
}