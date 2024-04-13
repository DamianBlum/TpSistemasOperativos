#ifndef CPU_H_
#define CPU_H_

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <utils/configUtils.h>
#include <commons/string.h>
#include <utils/sockets.h>

typedef enum
{
    DISPATCH = 0,
    INTERRUPT
} e_conexiones_kernel;

int main(int argc, char *argv[]);
void *servidor_dispatch(void *arg);
void *servidor_interrupt(void *arg);

#endif