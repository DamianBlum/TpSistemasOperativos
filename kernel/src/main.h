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

int main(int argc, char *argv[]);
int generar_clientes();
void *atender_servidor_io(void *arg);
void mostrar_menu(t_log logger);

#endif