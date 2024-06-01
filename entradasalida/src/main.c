#include "main.h"

t_log *logger;
t_config *config;

// Conexion con kernel
int cliente_kernel;

// Conexion con memoria
int cliente_memoria;

// hilos
t_list *lista_de_hilos;

int main(int argc, char *argv[])
{
    // Hacer un while que segun la cantidad de archivos de configuracion que haya, va a crear una interfaz por cada uno de ellos
    // Listar todo los archivos configs en la ruta actual
    logger = iniciar_logger("e-s.log", "ENTRADA-SALIDA", argc, argv);
    lista_de_hilos = list_create();

    DIR *d;
    struct dirent *directorio;
    d = opendir("./configs");
    if (d == NULL)
    {
        log_error(logger, "No se pudo abrir el directorio.");
        return EXIT_FAILURE;
    }
    int i = 0;
    while (directorio = readdir(d))
    {
        if (string_ends_with(directorio->d_name, ".config"))
        {
            t_interfaz_default *interfaz = crear_nueva_interfaz(directorio->d_name);
            pthread_t hilo_para_atender_interfaz;
            pthread_create(&hilo_para_atender_interfaz, NULL, manejo_de_interfaz, interfaz);
            list_add(lista_de_hilos, hilo_para_atender_interfaz);
            i++;
        }
    }
    closedir(d);

    while (!list_is_empty(lista_de_hilos))
    {
        pthread_t hilo = list_remove(lista_de_hilos, 0);
        pthread_join(hilo, NULL);
    }

    destruir_config(config);
    destruir_logger(logger);

    return EXIT_SUCCESS;
}

t_interfaz_default *crear_nueva_interfaz(char *nombre_archivo_config)
{
    char *nombre_archivo_con_carpeta = string_duplicate("./configs/");
    string_append(&nombre_archivo_con_carpeta, nombre_archivo_config);

    // creo cosas
    t_config *config = iniciar_config(nombre_archivo_con_carpeta);
    t_interfaz_default *interfaz = malloc(sizeof(t_interfaz_default));

    // agrego los datos de la interfaz default
    interfaz->nombre = string_split(nombre_archivo_config, ".")[0];
    interfaz->tipo_interfaz = convertir_tipo_interfaz_enum(config_get_string_value(config, "TIPO_INTERFAZ"));
    interfaz->conexion_kernel = crear_conexion(config, "IP_KERNEL", "PUERTO_KERNEL", logger);
    interfaz->accesos = 0;

    // agrego los datos de la interfaz especifica segun corresponda
    switch (interfaz->tipo_interfaz)
    {
    case GENERICA:
        t_interfaz_generica *tig = malloc(sizeof(t_interfaz_generica));
        tig->tiempo_unidad_trabajo = (uint32_t)config_get_long_value(config, "TIEMPO_UNIDAD_TRABAJO");
        interfaz->configs_especificas = tig;
        break;
    case STDIN:
        t_interfaz_stdin *tisin = malloc(sizeof(t_interfaz_stdin));
        tisin->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "IP_MEMORIA", logger);
        interfaz->configs_especificas = tisin;
        break;
    case STDOUT:
        t_interfaz_stdout *tisout = malloc(sizeof(t_interfaz_stdout));
        tisout->tiempo_unidad_trabajo = (uint32_t)config_get_long_value(config, "TIEMPO_UNIDAD_TRABAJO");
        tisout->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "IP_MEMORIA", logger);
        interfaz->configs_especificas = tisout;
        break;
    case DIALFS:
        t_interfaz_dialfs *tid = malloc(sizeof(t_interfaz_dialfs));
        tid->tiempo_unidad_trabajo = (uint32_t)config_get_long_value(config, "TIEMPO_UNIDAD_TRABAJO");
        tid->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "IP_MEMORIA", logger);
        tid->path_base_dialfs = (uint32_t)config_get_long_value(config, "PATH_BASE_DIALFS");
        tid->block_size = (uint32_t)config_get_long_value(config, "BLOCK_SIZE");
        tid->block_count = (uint32_t)config_get_long_value(config, "BLOCK_COUNT");
        tid->retraso_compactacion = (uint32_t)config_get_long_value(config, "RETRASO_COMPACTACION");
        break;
    default:
        break;
    }

    enviar_mensaje(interfaz->nombre, interfaz->conexion_kernel, logger);
    recibir_operacion(interfaz->conexion_kernel, logger);
    t_list *paquete = recibir_paquete(interfaz->conexion_kernel, logger);
    int resultado = (uint8_t)list_get(paquete, 0);
    if (resultado)
    {
        log_error(logger, "No fue posible crear la interfaz.");
        return NULL;
    }
    log_debug(logger, "Interfaz creada correctamente de tipo %s.", config_get_string_value(config, "TIPO_INTERFAZ"));
    destruir_config(config);
    free(nombre_archivo_con_carpeta);

    return interfaz;
}

e_tipo_interfaz convertir_tipo_interfaz_enum(char *tipo_interfaz)
{
    e_tipo_interfaz e_tipo_interfaz_;
    if (string_equals_ignore_case(tipo_interfaz, "GENERICA"))
    {
        e_tipo_interfaz_ = GENERICA;
    }
    else if (string_equals_ignore_case(tipo_interfaz, "STDIN"))
    {
        e_tipo_interfaz_ = STDIN;
    }
    else if (string_equals_ignore_case(tipo_interfaz, "STDOUT"))
    {
        e_tipo_interfaz_ = STDOUT;
    }
    else if (string_equals_ignore_case(tipo_interfaz, "DIALFS"))
    {
        e_tipo_interfaz_ = DIALFS;
    }

    return e_tipo_interfaz_;
}

int ejecutar_instruccion(char *nombre_instruccion, t_interfaz_default *interfaz, t_list *datos_desde_kernel)
{
    int ejecuto_correctamente = 0; // 0 = Incorrecta o No esta asociada a la interfaz que tengo.
                                   // 1 = Ejecuto correctamente.
                                   // 2 = Falla en la ejecucion.
    switch (interfaz->tipo_interfaz)
    {
    case GENERICA:
        if (string_equals_ignore_case(nombre_instruccion, "IO_GEN_SLEEP"))
        {
            uint32_t cantidad_de_esperas = list_get(datos_desde_kernel, 1);
            uint32_t tiempo_espera = (uint32_t)((t_interfaz_generica *)interfaz->configs_especificas)->tiempo_unidad_trabajo;

            sleep(tiempo_espera * cantidad_de_esperas);

            ejecuto_correctamente = 1;
        }
        else
        {
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz generica.", nombre_instruccion);
            ejecuto_correctamente = 0;
        }
        break;
    case STDIN:
        if (string_equals_ignore_case(nombre_instruccion, "IO_STDIN_READ"))
        {
            hacer_io_stdin_read(lista);
            ejecuto_correctamente = 1;
        }
        else
        {
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz STDIN.", nombre_instruccion);
            ejecuto_correctamente = 0;
        }
        break;
    case STDOUT:
        if (string_equals_ignore_case(nombre_instruccion, "IO_STDOUT_WRITE"))
        {
            hacer_io_stdout_write(lista);
        }
        else
        {
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz STDOUT.", nombre_instruccion);
            ejecuto_correctamente = 0;
        }
        break;
    case DIALFS:
        if (string_equals_ignore_case(nombre_instruccion, "IO_FS_CREATE"))
        {
            hacer_io_fs_create(lista);
            ejecuto_correctamente = 1;
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_DELETE"))
        {
            hacer_io_fs_delete(lista);
            ejecuto_correctamente = 1;
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_TRUNCATE"))
        {
            hacer_io_fs_truncate(lista);
            ejecuto_correctamente = 1;
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_WRITE"))
        {
            hacer_io_fs_write(lista);
            ejecuto_correctamente = 1;
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_READ"))
        {
            hacer_io_fs_read(lista);
            ejecuto_correctamente = 1;
        }
        else
        {
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz DIALFS.", nombre_instruccion);
            ejecuto_correctamente = 0;
        }
        break;

    default:
        break;
    }
    return ejecuto_correctamente;
}

void hacer_io_stdin_read(t_list *lista)
{
    char *texto_ingresado;
    texto_ingresado = readline(">");
    // ahora q lo lei, lo voy a acortar si es necesario

    char *texto_chiquito = string_substring_until(texto_ingresado, (int)list_get(lista, 2) / 4);

    t_paquete *paquete_para_mem = crear_paquete();
    agregar_a_paquete(paquete_para_mem, PEDIDO_ESCRITURA, sizeof(uint8_t));
    agregar_a_paquete(paquete_para_mem, list_get(lista, 1), sizeof(uint32_t)); // Reg direc logica (en realidad aca mepa q recivo la fisica)
    agregar_a_paquete(paquete_para_mem, list_get(lista, 2), sizeof(uint32_t)); // Reg tam
    agregar_a_paquete(paquete_para_mem, list_get(lista, 3), sizeof(uint8_t));  // PID
    agregar_a_paquete(paquete_para_mem, texto_chiquito, strlen(texto_chiquito));
    enviar_paquete(paquete_para_mem, cliente_memoria, logger);

    // aca podria esperar a ver q me dice memoria sobre esto
}

void hacer_io_stdout_write(t_list *lista)
{ // lista = [nombre_instruccion, reg_dir_logica, reg_tam, PID]

    // consumir una unidad de trabajo

    // leer de memoria
}

void hacer_io_fs_read(t_list *lista)
{
}

void hacer_io_fs_create(t_list *lista)
{
}

void hacer_io_fs_delete(t_list *lista)
{
}

void hacer_io_fs_truncate(t_list *lista)
{
}

void hacer_io_fs_write(t_list *lista)
{
}

void manejo_de_interfaz(void *args)
{
    t_interfaz_default *interfaz = (t_interfaz_default *)args;

    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(interfaz->conexion_kernel, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(interfaz->conexion_kernel, logger);
            log_info(logger, "Desde cliente %d: Recibi el mensaje: %s.", interfaz->conexion_kernel, mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(interfaz->conexion_kernel, logger);
            log_info(logger, "Desde cliente %d: Recibi un paquete.", interfaz->conexion_kernel);
            char *nombre_instruccion = list_get(lista, 0);
            int resultado = ejecutar_instruccion(nombre_instruccion, interfaz->tipo_interfaz, lista);
            enviar_mensaje(string_itoa(resultado), interfaz->conexion_kernel, logger);
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", interfaz->conexion_kernel);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente %d: Recibi una operacion rara (%d), termino el servidor.", interfaz->conexion_kernel, operacion);
            return EXIT_FAILURE;
            break;
        }
        liberar_conexion(interfaz->conexion_kernel, logger);
        liberar_conexion(interfaz->conexion_memoria, logger);
    }
    return EXIT_SUCCESS;
}