#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <utils/configUtils.h>
#include <commons/string.h>
#include <utils/sockets.h>
#include <pthread.h>
#include <dirent.h>

typedef enum
{
    GENERICA = 0,
    STDIN,
    STDOUT,
    DIALFS,
} e_tipo_interfaz;
typedef struct
{
    char* nombre;
    //char* tipo_interfaz;
    uint32_t  tiempo_unidad_trabajo;
    char* ip_kernel;
    int conexion_kernel;
    int conexion_memoria;
    char* path_base_dialfs;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t retraso_compactacion;
    e_tipo_interfaz tipo_interfaz; 
} t_interfaz;

int main(int argc, char *argv[]);
void hacer_io_stdin_read(t_list* lista);
void hacer_io_stdout_write(t_list* lista);
void hacer_io_sleep(t_list* lista);
void hacer_io_fs_read(t_list* lista);
void hacer_io_fs_create(t_list* lista);
void hacer_io_fs_delete(t_list* lista);
void hacer_io_fs_truncate(t_list* lista);
void hacer_io_fs_write(t_list* lista);
void manejo_de_interfaz(void* args);
e_tipo_interfaz convertir_tipo_interfaz_enum(char* tipo_interfaz);
#endif