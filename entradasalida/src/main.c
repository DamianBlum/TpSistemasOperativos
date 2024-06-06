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
        tisin->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
        interfaz->configs_especificas = tisin;
        break;
    case STDOUT:
        t_interfaz_stdout *tisout = malloc(sizeof(t_interfaz_stdout));
        tisout->tiempo_unidad_trabajo = (uint32_t)config_get_long_value(config, "TIEMPO_UNIDAD_TRABAJO");
        tisout->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
        interfaz->configs_especificas = tisout;
        break;
    case DIALFS:
        t_interfaz_dialfs *tid = malloc(sizeof(t_interfaz_dialfs));
        tid->tiempo_unidad_trabajo = (uint32_t)config_get_long_value(config, "TIEMPO_UNIDAD_TRABAJO");
        tid->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
        tid->path_base_dialfs = config_get_string_value(config, "PATH_BASE_DIALFS");
        tid->block_size = (uint32_t)config_get_long_value(config, "BLOCK_SIZE");
        tid->block_count = (uint32_t)config_get_long_value(config, "BLOCK_COUNT");
        tid->retraso_compactacion = (uint32_t)config_get_long_value(config, "RETRASO_COMPACTACION");

        // creo las estructuras necesarias (si ya estan creadas no pasa nada)
        // 1- bloques.dat
        char *path_bloques = string_duplicate(tid->path_base_dialfs);
        string_append(&path_bloques, "bloques.dat");
        FILE *fbloques = fopen(path_bloques, "ab");
        log_debug(logger, "Se creo el archivo de bloques en el path: %s", path_bloques);
        truncate(path_bloques, tid->block_count * tid->block_size);
        log_debug(logger, "Se trunco el archivo para que tenga size: %u", tid->block_count * tid->block_size);

        // 2- bitmap.dat

        // 3- metadata (1 por archivo) => nombreArchivo.config
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
    log_trace(logger, "(%s|%u): me llego la instruccion: %s", interfaz->nombre, interfaz->tipo_interfaz, nombre_instruccion);

    switch (interfaz->tipo_interfaz)
    {
    case GENERICA:
        if (string_equals_ignore_case(nombre_instruccion, "IO_GEN_SLEEP"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_GEN_SLEEP.", interfaz->nombre, interfaz->tipo_interfaz);
            uint32_t cantidad_de_esperas = list_get(datos_desde_kernel, 1);
            uint32_t tiempo_espera = (uint32_t)((t_interfaz_generica *)interfaz->configs_especificas)->tiempo_unidad_trabajo;
            uint32_t espera_total = tiempo_espera * cantidad_de_esperas;

            consumir_tiempo_trabajo(espera_total, interfaz);

            ejecuto_correctamente = 1;
        }
        else
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz generica.", nombre_instruccion);
        break;
    case STDIN:
        if (string_equals_ignore_case(nombre_instruccion, "IO_STDIN_READ"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_STDIN_READ.", interfaz->nombre, interfaz->tipo_interfaz);
            char *texto_ingresado;
            texto_ingresado = readline(">");

            uint32_t dir = list_get(datos_desde_kernel, 1);
            uint32_t tamanio_registro = list_get(datos_desde_kernel, 2);
            uint32_t pid = list_get(datos_desde_kernel, 3);
            log_debug(logger, "(%s|%u): Texto ingresado: %s | Direccion fisica: %u | Size registro: %u | PID: %u.", interfaz->nombre, interfaz->tipo_interfaz, texto_ingresado, dir, tamanio_registro, pid);

            // ahora q lo lei, lo voy a acortar si es necesario
            char *texto_chiquito = string_substring_until(texto_ingresado, (int)tamanio_registro / 4);
            log_debug(logger, "(%s|%u): Texto ingresado despues de cortarlo: %s", interfaz->nombre, interfaz->tipo_interfaz, texto_chiquito);

            log_trace(logger, "(%s|%u): Le voy a enviar a memoria el texto para que lo escriba.", interfaz->nombre, interfaz->tipo_interfaz);
            t_paquete *paquete_para_mem = crear_paquete();
            agregar_a_paquete(paquete_para_mem, PEDIDO_ESCRITURA, sizeof(uint8_t));
            agregar_a_paquete(paquete_para_mem, dir, sizeof(uint32_t));              // Reg direc logica (en realidad aca mepa q recivo la fisica)
            agregar_a_paquete(paquete_para_mem, tamanio_registro, sizeof(uint32_t)); // Reg tam
            agregar_a_paquete(paquete_para_mem, pid, sizeof(uint32_t));              // PID
            agregar_a_paquete(paquete_para_mem, texto_chiquito, strlen(texto_chiquito));
            enviar_paquete(paquete_para_mem, (int)((t_interfaz_stdin *)interfaz->configs_especificas)->conexion_memoria, logger);

            // aca podria esperar a ver q me dice memoria sobre esto

            ejecuto_correctamente = 1;
        }
        else
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz generica.", nombre_instruccion);
        break;
    case STDOUT:
        if (string_equals_ignore_case(nombre_instruccion, "IO_STDOUT_WRITE"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_STDOUT_WRITE.", interfaz->nombre, interfaz->tipo_interfaz);
            uint32_t dir_fisica = list_get(datos_desde_kernel, 1);
            uint32_t tam_dato = list_get(datos_desde_kernel, 2);
            uint32_t pid = list_get(datos_desde_kernel, 3);
            t_interfaz_stdout *tisout = (t_interfaz_stdout *)interfaz->configs_especificas;
            int cm = (int)tisout->conexion_memoria;

            log_debug(logger, "(%s|%u): Direccion fisica: %u | Size dato: %u | PID: %u.", interfaz->nombre, interfaz->tipo_interfaz, dir_fisica, tam_dato, pid);

            // consumo 1 unidad de trabajo
            consumir_tiempo_trabajo(tisout->tiempo_unidad_trabajo, interfaz);

            // leo la direccion en memoria
            log_trace(logger, "(%s|%u): Le voy a pedir a memoria la lectura de la direccion fisica %u.", interfaz->nombre, interfaz->tipo_interfaz, dir_fisica);
            t_paquete *pm = crear_paquete();
            agregar_a_paquete(pm, PEDIDO_LECTURA, sizeof(PEDIDO_LECTURA));
            agregar_a_paquete(pm, dir_fisica, sizeof(uint32_t));
            agregar_a_paquete(pm, tam_dato, sizeof(uint32_t));
            agregar_a_paquete(pm, pid, sizeof(uint32_t));
            enviar_paquete(pm, cm, logger);

            // esperar y printear el valor obtenido
            recibir_operacion(cm, logger);
            char *mensaje_obtenido = recibir_mensaje(cm, logger);
            log_info(logger, "(%s|%u): Resultado de la lectura a memoria: %s", interfaz->nombre, interfaz->tipo_interfaz, mensaje_obtenido);
            // log_info(logger, "Print del valor leido en memoria: %s", mensaje_obtenido);

            ejecuto_correctamente = 1;
        }
        else
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz generica.", nombre_instruccion);
        break;
    case DIALFS:
        // siempre consumo 1 unidad de trabajo
        t_interfaz_dialfs *idialfs = (t_interfaz_dialfs *)interfaz->configs_especificas;
        consumir_tiempo_trabajo(idialfs->tiempo_unidad_trabajo, interfaz);

        if (string_equals_ignore_case(nombre_instruccion, "IO_FS_CREATE"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_FS_CREATE.", interfaz->nombre, interfaz->tipo_interfaz);
            ejecuto_correctamente = 1;
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_DELETE"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_FS_DELETE.", interfaz->nombre, interfaz->tipo_interfaz);
            ejecuto_correctamente = 1;
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_TRUNCATE"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_FS_TRUNCATE.", interfaz->nombre, interfaz->tipo_interfaz);
            ejecuto_correctamente = 1;
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_WRITE"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_FS_WRITE.", interfaz->nombre, interfaz->tipo_interfaz);
            ejecuto_correctamente = 1;
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_READ"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_FS_READ.", interfaz->nombre, interfaz->tipo_interfaz);
            ejecuto_correctamente = 1;
        }
        else
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz generica.", nombre_instruccion);
        break;

    default:
        break;
    }
    return ejecuto_correctamente;
}

void consumir_tiempo_trabajo(uint32_t tiempo_en_ms, t_interfaz_default *interfaz)
{
    uint32_t tiempo_en_s = tiempo_en_ms / 1000;
    log_debug(logger, "(%s|%u): Tiempo a dormir en ms: %u | en s: %u", interfaz->nombre, interfaz->tipo_interfaz, tiempo_en_ms, tiempo_en_s);
    log_trace(logger, "(%s|%u): Voy a hacer sleep por %u segundos.", interfaz->nombre, interfaz->tipo_interfaz, tiempo_en_s);
    sleep(tiempo_en_s);
    log_trace(logger, "(%s|%u): Termino el sleep.", interfaz->nombre, interfaz->tipo_interfaz);
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
            log_info(logger, "Desde cliente %d | %s: Recibi el mensaje: %s.", interfaz->conexion_kernel, interfaz->nombre, mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(interfaz->conexion_kernel, logger);
            log_info(logger, "Desde cliente %d | %s: Recibi un paquete.", interfaz->conexion_kernel, interfaz->nombre);
            char *nombre_instruccion = list_get(lista, 0);
            int resultado = ejecutar_instruccion(nombre_instruccion, interfaz, lista);

            // le respondo a kernel
            log_trace(logger, "(%s|%u): Le voy a responder a kernel el resultado de la ejecucion: %u.", interfaz->nombre, interfaz->tipo_interfaz, resultado);
            t_paquete *paquete_resp_kernel = crear_paquete();
            agregar_a_paquete(paquete_resp_kernel, (uint8_t)resultado, sizeof(uint8_t));
            enviar_paquete(paquete_resp_kernel, interfaz->conexion_kernel, logger);
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d | %s.", interfaz->conexion_kernel, interfaz->nombre);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente %d | %s: Recibi una operacion rara (%d), termino el servidor.", interfaz->conexion_kernel, interfaz->nombre, operacion);
            return EXIT_FAILURE;
            break;
        }
    }
    liberar_conexion(interfaz->conexion_kernel, logger);
    // liberar_conexion(interfaz->conexion_memoria, logger);
    return EXIT_SUCCESS;
}