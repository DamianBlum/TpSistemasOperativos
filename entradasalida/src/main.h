#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <utils/configUtils.h>
#include <commons/string.h>
#include <readline/readline.h>
#include <utils/sockets.h>
#include <pthread.h>
#include <dirent.h>
#include <utils/operacionMemoriaUtils.h>
#include <commons/collections/list.h>

// Estructuras para el manejo de interfaces
typedef enum
{
    GENERICA = 0,
    STDIN,
    STDOUT,
    DIALFS,
} e_tipo_interfaz;

typedef struct
{
    char *nombre;
    e_tipo_interfaz tipo_interfaz;
    uint32_t conexion_kernel;
    uint32_t accesos;
    void *configs_especificas;
} t_interfaz_default;

typedef struct
{
    uint32_t tiempo_unidad_trabajo;
} t_interfaz_generica;

typedef struct
{
    uint32_t conexion_memoria;
} t_interfaz_stdin;

typedef struct
{
    uint32_t tiempo_unidad_trabajo;
    uint32_t conexion_memoria;
} t_interfaz_stdout;

typedef struct
{
    uint32_t tiempo_unidad_trabajo;
    uint32_t conexion_memoria;
    char *path_base_dialfs;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t retraso_compactacion;
} t_interfaz_dialfs;

int main(int argc, char *argv[]);
void hacer_io_stdin_read(t_list *lista);
void hacer_io_stdout_write(t_list *lista);
void hacer_io_sleep(t_list *lista);
void hacer_io_fs_read(t_list *lista);
void hacer_io_fs_create(t_list *lista);
void hacer_io_fs_delete(t_list *lista);
void hacer_io_fs_truncate(t_list *lista);
void hacer_io_fs_write(t_list *lista);
void manejo_de_interfaz(void *args);
e_tipo_interfaz convertir_tipo_interfaz_enum(char *tipo_interfaz);
t_interfaz_default *crear_nueva_interfaz(char *nombre_archivo_config);
void consumir_tiempo_trabajo(uint32_t tiempo_en_ms);
int ejecutar_instruccion(char *nombre_instruccion, t_interfaz_default *interfaz, t_list *datos_desde_kernel);
#endif