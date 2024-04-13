#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <utils/configUtils.h>
#include <commons/string.h>
#include <utils/sockets.h>

typedef enum
{
    SOY_CPU = 0,
    SOY_KERNEL,
    SOY_IO
} e_quien_soy;

int main(int argc, char *argv[]);
void *servidor_kernel(void *arg);
void *servidor_cpu(void *arg);
void *servidor_entradasalida(void *arg);

#endif