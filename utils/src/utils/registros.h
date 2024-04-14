#ifndef REGISTROS_H_
#define REGISTROS_H_

#include <netdb.h>

typedef enum
{
    // Segmentos
    SS,
    DS,
    CS,
    // Uso comun
    AX,
    BX,
    CX,
    DX,
    // Cantidad de registros
    CANTIDAD_REGISTROS,
} e_registros;