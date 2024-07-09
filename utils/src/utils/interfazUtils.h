#ifndef INTERFAZUTILS_H_
#define INTERFAZUTILS_H_

typedef enum
{
    GENERICA = 0,
    STDIN,
    STDOUT,
    DIALFS_CREATE,
    DIALFS_DELETE,
    DIALFS_TRUNCATE,
    DIALFS_WRITE,
    DIALFS_READ,
} e_tipo_interfaz;

#endif