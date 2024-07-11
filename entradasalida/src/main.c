#include "main.h"

t_log *logger;

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
    char** nombre = string_split(nombre_archivo_config, ".");
    interfaz->nombre = string_duplicate(nombre[0]);
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
        log_debug(logger, "Como soy una interfaz STDIN voy a crear la conexion con memoria.");
        tisin->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
        interfaz->configs_especificas = tisin;
        break;
    case STDOUT:
        t_interfaz_stdout *tisout = malloc(sizeof(t_interfaz_stdout));
        log_debug(logger, "Como soy una interfaz STDOUT voy a crear la conexion con memoria.");
        tisout->tiempo_unidad_trabajo = (uint32_t)config_get_long_value(config, "TIEMPO_UNIDAD_TRABAJO");
        tisout->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
        interfaz->configs_especificas = tisout;
        break;
    case DIALFS:
        t_interfaz_dialfs *tid = malloc(sizeof(t_interfaz_dialfs));
        tid->tiempo_unidad_trabajo = (uint32_t)config_get_long_value(config, "TIEMPO_UNIDAD_TRABAJO");
        tid->conexion_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
        tid->path_base_dialfs = string_duplicate(config_get_string_value(config, "PATH_BASE_DIALFS"));
        tid->block_size = (uint32_t)config_get_long_value(config, "BLOCK_SIZE");
        tid->block_count = (uint32_t)config_get_long_value(config, "BLOCK_COUNT");
        tid->retraso_compactacion = (uint32_t)config_get_long_value(config, "RETRASO_COMPACTACION");
        interfaz->configs_especificas = tid; // casi me olvido de esto je

        // creo las estructuras necesarias (si ya estan creadas no pasa nada)
        // 1- bloques.dat
        char *path_bloques = string_duplicate(tid->path_base_dialfs);
        string_append(&path_bloques, "bloques.dat");
        tid->bloques = crear_bloques(path_bloques, tid->block_count, tid->block_size, logger);

        // 2- bitmap.dat
        char *path_bitmap = string_duplicate(tid->path_base_dialfs);
        string_append(&path_bitmap, "bitmap.dat");
        tid->bitmap = crear_bitmap(path_bitmap, tid->block_count, logger);

        free(path_bloques);
        free(path_bitmap);
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
    string_array_destroy(nombre);
    list_destroy(paquete); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
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

int ejecutar_instruccion(t_interfaz_default *interfaz, t_list *datos_desde_kernel)
{
    char *nombre_instruccion = list_get(datos_desde_kernel, 0);
    uint32_t pid = list_get(datos_desde_kernel, 1);

    int ejecuto_correctamente = 0; // 0 = Incorrecta o No esta asociada a la interfaz que tengo.
                                   // 1 = Ejecuto correctamente.
                                   // 2 = Falla en la ejecucion.

    log_trace(logger, "(%s|%u): me llego la instruccion: |%s|", interfaz->nombre, interfaz->tipo_interfaz, nombre_instruccion);

    log_info(logger, "PID: <%u> - Operacion: <%s>", pid, nombre_instruccion); // log obligatorio

    switch (interfaz->tipo_interfaz)
    {
    case GENERICA:
        if (string_equals_ignore_case(nombre_instruccion, "IO_GEN_SLEEP"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_GEN_SLEEP.", interfaz->nombre, interfaz->tipo_interfaz);
            uint32_t cantidad_de_esperas = list_get(datos_desde_kernel, 2);
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
            t_interfaz_stdin *tisdin = (t_interfaz_stdin *)interfaz->configs_especificas;
            log_trace(logger, "(%s|%u): Entre a IO_STDIN_READ.", interfaz->nombre, interfaz->tipo_interfaz);
            char *texto_ingresado;
            texto_ingresado = readline(">");

            uint32_t dir = (uint32_t)list_get(datos_desde_kernel, 2);
            uint32_t tamanio_registro = list_get(datos_desde_kernel, 3);
            log_debug(logger, "(%s|%u): Texto ingresado: %s | Direccion fisica: %u | Size registro: %u | PID: %u.", interfaz->nombre, interfaz->tipo_interfaz, texto_ingresado, dir, tamanio_registro, pid);

            // ahora q lo lei, lo voy a acortar si es necesario
            char *texto_chiquito = string_substring_until(texto_ingresado, tamanio_registro); // para dejar lugar al \0
            log_debug(logger, "(%s|%u): Texto ingresado despues de cortarlo: %s", interfaz->nombre, interfaz->tipo_interfaz, texto_chiquito);

            log_trace(logger, "(%s|%u): Le voy a enviar a memoria el texto para que lo escriba.", interfaz->nombre, interfaz->tipo_interfaz);
            t_paquete *paquete_para_mem = crear_paquete();
            agregar_a_paquete(paquete_para_mem, PEDIDO_ESCRITURA, sizeof(uint8_t));
            agregar_a_paquete(paquete_para_mem, dir, sizeof(uint32_t));              // Reg direc logica (en realidad aca mepa q recivo la fisica)
            agregar_a_paquete(paquete_para_mem, tamanio_registro, sizeof(uint32_t)); // Reg tam
            agregar_a_paquete(paquete_para_mem, pid, sizeof(uint32_t));              // PID
            agregar_a_paquete(paquete_para_mem, texto_chiquito, strlen(texto_chiquito) + 1);
            agregar_a_paquete(paquete_para_mem, 0, sizeof(uint8_t));
            int cm = (int)tisdin->conexion_memoria;
            enviar_paquete(paquete_para_mem, cm, logger);

            // aca podria esperar a ver q me dice memoria sobre esto
            recibir_operacion(cm, logger); 

            char *mensaje_memoria = recibir_mensaje(cm, logger);
            log_debug(logger, "(%s|%u): Resultado de la escritura en memoria: %s", interfaz->nombre, interfaz->tipo_interfaz, mensaje_memoria);
            ejecuto_correctamente = 1;
            free(texto_chiquito); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
            free(texto_ingresado); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
            free(mensaje_memoria); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
        }
        else
            log_error(logger, "ERROR: la instruccion pedida (%s) no corresponde a una interfaz generica.", nombre_instruccion);
        break;
    case STDOUT:
        if (string_equals_ignore_case(nombre_instruccion, "IO_STDOUT_WRITE"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_STDOUT_WRITE.", interfaz->nombre, interfaz->tipo_interfaz);
            uint32_t dir_fisica = list_get(datos_desde_kernel, 2);
            uint32_t tam_dato = list_get(datos_desde_kernel, 3);
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
            agregar_a_paquete(pm, 0, sizeof(uint8_t));
            enviar_paquete(pm, cm, logger);

            // esperar y printear el valor obtenido
            recibir_operacion(cm, logger);
            t_list *lista_recibida = recibir_paquete(cm, logger);
            char *mensaje_obtenido = list_get(lista_recibida, 0);
            log_info(logger, "(%s|%u): Resultado de la lectura a memoria: %s", interfaz->nombre, interfaz->tipo_interfaz, mensaje_obtenido);
            // log_info(logger, "Print del valor leido en memoria: %s", mensaje_obtenido);

            ejecuto_correctamente = 1;
            list_destroy(lista_recibida); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
            free(mensaje_obtenido); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
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

            char *nombre_archivo = list_get(datos_desde_kernel, 2);

            log_info(logger, "PID: <%u> - Crear Archivo: <%s>", pid, nombre_archivo); // log obligatorio

            ejecuto_correctamente = crear_archivo(idialfs, nombre_archivo);
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_DELETE"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_FS_DELETE.", interfaz->nombre, interfaz->tipo_interfaz);
            char *nombre_archivo = list_get(datos_desde_kernel, 2);

            log_info(logger, "PID: <%u> - Eliminar Archivo: <%s>", pid, nombre_archivo); // log obligatorio

            ejecuto_correctamente = borrar_archivo(idialfs, nombre_archivo);
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_TRUNCATE"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_FS_TRUNCATE.", interfaz->nombre, interfaz->tipo_interfaz);
            char *nombre_archivo = list_get(datos_desde_kernel, 2);
            uint32_t nuevo_size = list_get(datos_desde_kernel, 3);

            log_info(logger, "PID: <%u> - Truncar Archivo: <%s> - Tamaño: <%u>", pid, nombre_archivo, nuevo_size); // log obligatorio

            ejecuto_correctamente = truncar_archivo(idialfs, nombre_archivo, nuevo_size, pid);
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_WRITE"))
        { // IO_FS_WRITE Int4 notas.txt AX ECX EDX
            log_trace(logger, "(%s|%u): Entre a IO_FS_WRITE.", interfaz->nombre, interfaz->tipo_interfaz);
            char *nombre_archivo = list_get(datos_desde_kernel, 2);
            uint32_t df = list_get(datos_desde_kernel, 3);
            uint32_t size_dato = list_get(datos_desde_kernel, 4);
            uint32_t puntero_archivo = list_get(datos_desde_kernel, 5);
            int cm = (int)idialfs->conexion_memoria;
            log_info(logger, "PID: <%u> - Leer Archivo: <%s> - Tamaño a Leer: <%u> - Puntero Archivo: <%u>", pid, nombre_archivo, size_dato, puntero_archivo); // log obligatorio

            // leo la direccion en memoria para conseguir el dato a escribir en el archivo
            log_trace(logger, "(%s|%u): Le voy a pedir a memoria la lectura de la direccion fisica %u.", interfaz->nombre, interfaz->tipo_interfaz, df);
            t_paquete *pm = crear_paquete();
            agregar_a_paquete(pm, PEDIDO_LECTURA, sizeof(PEDIDO_LECTURA));
            agregar_a_paquete(pm, df, sizeof(uint32_t));
            agregar_a_paquete(pm, size_dato, sizeof(uint32_t));
            agregar_a_paquete(pm, pid, sizeof(uint32_t));
            agregar_a_paquete(pm, 0, sizeof(uint8_t));
            enviar_paquete(pm, cm, logger);

            // esperar y printear el valor obtenido
            recibir_operacion(cm, logger);
            t_list *lista_recibida = recibir_paquete(cm, logger);
            char *mensaje_obtenido = list_get(lista_recibida, 0);
            log_debug(logger, "(%s|%u): Resultado de la lectura a memoria: %s", interfaz->nombre, interfaz->tipo_interfaz, mensaje_obtenido);

            ejecuto_correctamente = escribir_en_archivo(idialfs, nombre_archivo, size_dato, puntero_archivo, mensaje_obtenido);
        }
        else if (string_equals_ignore_case(nombre_instruccion, "IO_FS_READ"))
        {
            log_trace(logger, "(%s|%u): Entre a IO_FS_READ.", interfaz->nombre, interfaz->tipo_interfaz);

            char *nombre_archivo = list_get(datos_desde_kernel, 2);
            uint32_t dir_fisica_mem = list_get(datos_desde_kernel, 3);
            uint32_t size_dato = list_get(datos_desde_kernel, 4);
            uint32_t puntero_archivo = list_get(datos_desde_kernel, 5);

            log_info(logger, "PID: <%u> - Escribir Archivo: <%s> - Tamaño a Escribir: <%u> - Puntero Archivo: <%u>", pid, nombre_archivo, size_dato, puntero_archivo); // log obligatorio

            char *resultado = (char*)leer_en_archivo(idialfs, nombre_archivo, size_dato, puntero_archivo);

            t_paquete *paquete_para_mem = crear_paquete();
            agregar_a_paquete(paquete_para_mem, PEDIDO_ESCRITURA, sizeof(uint8_t));
            agregar_a_paquete(paquete_para_mem, dir_fisica_mem, sizeof(uint32_t)); // Reg direc logica (en realidad aca mepa q recivo la fisica)
            agregar_a_paquete(paquete_para_mem, size_dato, sizeof(uint32_t));      // Reg tam
            agregar_a_paquete(paquete_para_mem, pid, sizeof(uint32_t));            // PID
            agregar_a_paquete(paquete_para_mem, resultado, strlen(resultado)+1);
            agregar_a_paquete(paquete_para_mem, 0, sizeof(uint8_t)); // 0 char* - 1 Numero
            enviar_paquete(paquete_para_mem, (int)((t_interfaz_dialfs *)interfaz->configs_especificas)->conexion_memoria, logger);

            recibir_operacion((int)((t_interfaz_dialfs *)interfaz->configs_especificas)->conexion_memoria, logger);
            char *respuesta_memoria = recibir_mensaje((int)((t_interfaz_dialfs *)interfaz->configs_especificas)->conexion_memoria, logger);
            if (string_equals_ignore_case(respuesta_memoria, "OK"))
                ejecuto_correctamente = 1;
            free(respuesta_memoria); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
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
    uint32_t tiempo_en_micro_s = tiempo_en_ms * 1000;
    log_trace(logger, "(%s|%u): Voy a hacer sleep por %u milisegundos.", interfaz->nombre, interfaz->tipo_interfaz, tiempo_en_ms);
    usleep(tiempo_en_micro_s);
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
            free(mensaje);
            // realemente nunca le va a mandar un mensaje pero lo dejo por si acaso
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(interfaz->conexion_kernel, logger);
            log_info(logger, "Desde cliente %d | %s: Recibi un paquete.", interfaz->conexion_kernel, interfaz->nombre);
            int resultado = ejecutar_instruccion(interfaz, lista);
            // habria que destruir la lista REVISAR
            // le respondo a kernel
            log_trace(logger, "(%s|%u): Le voy a responder a kernel el resultado de la ejecucion: %u.", interfaz->nombre, interfaz->tipo_interfaz, resultado);
            t_paquete *paquete_resp_kernel = crear_paquete();
            agregar_a_paquete(paquete_resp_kernel, (uint8_t)resultado, sizeof(uint8_t));
            enviar_paquete(paquete_resp_kernel, interfaz->conexion_kernel, logger);
            list_destroy(lista); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d | %s.", interfaz->conexion_kernel, interfaz->nombre);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente %d | %s: Recibi una operacion rara (%d), termino el servidor.", interfaz->conexion_kernel, interfaz->nombre, operacion);
            sigo_funcionando = 0;
            break;
        }
    }
    
    destruir_interfaz(interfaz); // esto no estaba puesto, despues ver si no rompe nada 10/7/24
    
    return EXIT_SUCCESS;
}

uint8_t crear_archivo(t_interfaz_dialfs *idial, char *nombre_archivo)
{ // solamente le asigno 1 bloque al crear un archivo

    // valido que el archivo ya no exista
    char *path_metadata = armar_path_metadata(nombre_archivo, idial->path_base_dialfs);
    log_debug(logger, "Path armado: %s", path_metadata);

    if (access(path_metadata, F_OK) == 0)
    {
        log_error(logger, "Ya existe el archivo %s, se cancela la creacion.", nombre_archivo);
        return 0;
    }

    // me fijo donde lo puedo ubicar
    for (uint32_t i = 0; i < idial->block_count; i++)
    {
        uint8_t hay_espacio = 1;
        if (esta_bloque_ocupado(idial->bitmap, i))
        {
            log_trace(logger, "Bloque %u ocupado, me quedan %u.", i, idial->block_count - (i + 1));
            hay_espacio = 0;
        }

        // log_debug(logger, "Bloque %u libre, voy a asignar el archivo ahi.", i);

        if (hay_espacio)
        { // si existe un lugar donde guardar el dato, lo escribo en el bitmap
            log_trace(logger, "Se encontro un espacio (bloque: %u) para guardar el archivo %s.", i, nombre_archivo);

            if (!ocupar_bloque(idial->bitmap, i))
                log_error(logger, "Error al intentar ocupar el bloque %u para el archivo %s", i, nombre_archivo); // por que habria error? REVISAR
            else
                log_debug(logger, "Se ocupo el bloque %u para el archivo %s", i, nombre_archivo);

            log_trace(logger, "Se reservo el bloque %u para el archivo %s.", i, nombre_archivo);

            // limpio el bloque del archivo
            limpiar_bloque(idial->bloques, i);

            // creo el archivo de metadata
            FILE *fmd = txt_open_for_append(path_metadata);
            txt_close_file(fmd);

            // creo un config con el archivo y le asigno las keys con sus valores
            t_config *c = config_create(path_metadata);
            config_set_value(c, "BLOQUE_INICIAL", string_itoa(i));
            config_set_value(c, "TAMANIO_ARCHIVO", string_itoa(0));
            config_save(c);
            destruir_config(c);

            // salgo del for
            break;
        }
        else if (i == idial->block_count - 1) // recorri todo el bitmap
        {
            log_error(logger, "No se encontro un espacio donde pueda guardar el archivo %s.", nombre_archivo);
            return 0;
        }
    }
    free(path_metadata);
    return 1;
}

uint8_t borrar_archivo(t_interfaz_dialfs *idial, char *nombre_archivo)
{
    // armo el path
    char *path_metadata = armar_path_metadata(nombre_archivo, idial->path_base_dialfs);

    // consigo el config
    t_config *c = config_create(path_metadata);
    uint32_t bloque_inicial = (uint32_t)config_get_int_value(c, "BLOQUE_INICIAL");
    uint32_t tamanio_archivo = (uint32_t)config_get_int_value(c, "TAMANIO_ARCHIVO");
    //Esto lo cambiaria por (double)tamanio_archivo / (double)idial->block_size 
    uint32_t cant_bloques = (uint32_t)ceil((double)tamanio_archivo / (double)idial->block_size); // ceil redondea para arriba

    limpiar_bitmap(idial, bloque_inicial, cant_bloques);

    // limpio todos los bloques de datos
    for (uint32_t i = bloque_inicial; i < bloque_inicial + cant_bloques; i++)
    {
        limpiar_bloque(idial->bloques, i);
    }

    // borro el .metadata
    uint8_t r = 1;

    if (remove(path_metadata))
        r = 0; // error al borrar el archivo
    config_destroy(c); // esto no estaba puesto, despues ver si no rompe nada 9/7/24
    free(path_metadata); // esto no estaba puesto, despues ver si no rompe nada 9/7/24
    return r;
}

uint8_t truncar_archivo(t_interfaz_dialfs *idial, char *nombre_archivo, uint32_t nuevo_size, uint32_t pid)
{
    uint8_t resultado = 0;
    // consigo la info del archivo
    char *path_metadata = armar_path_metadata(nombre_archivo, idial->path_base_dialfs);
    t_config *config = config_create(path_metadata);
    uint32_t bloque_inicial = (uint32_t)config_get_int_value(config, "BLOQUE_INICIAL");
    uint32_t size_archivo = (uint32_t)config_get_int_value(config, "TAMANIO_ARCHIVO");

    if (nuevo_size > size_archivo)
    { // en este caso me tengo q fijar que exista el espacio
        double bloque_a_agregar_sin_redondear = (double)(nuevo_size)/(double)(idial->block_size);
        uint32_t bloques_a_agregar = (uint32_t)ceil(bloque_a_agregar_sin_redondear);
        uint32_t pos_arranque = bloque_inicial + size_archivo; // esto es el primer bloque que le sigue al ultimo que ya tiene asignado el archivo
        uint8_t puedo_truncar = 1;

        // caso especial SOLAMENTE si el el truncate es para un archivo recien creado (size 0)
        if (size_archivo == 0)
        { // esto es xq aunque yo en el bitmap le asigno un bloque al archivo cuando lo creo, en la metadata le tengo que poner tamanio 0
            pos_arranque++;
            bloques_a_agregar--;
        }
        for (uint32_t i = pos_arranque; i < (pos_arranque+bloques_a_agregar); i++)
        { // se pueden dar 2 situacion por las cuales no pueda truncar:
            // 1- pido mas de lo que le queda al filesystem de espacio (desde mi pos actual, ej: hay 1024, estoy en la 700 y pido 500)
            // 2- hay bloques ocupados que necesitaria tomar para poder hacer el trunque (ej: estoy en 200, pido 50 y desde la 230 esta ocupado por otro archivo)
            if (i >= idial->block_count || esta_bloque_ocupado(idial->bitmap, i))
            {
                log_error(logger, "No se puede truncar porque el bloque %u esta ocupado por otro proceso (me faltaron %u bloques).", i, bloques_a_agregar - i);
                puedo_truncar = 0;
                break;
            }
        }

        // trunco si se puede
        if (puedo_truncar)
        {
            // seteo el bitmap
            for (uint32_t i = pos_arranque; i < pos_arranque + bloques_a_agregar; i++)
            {
                if (!ocupar_bloque(idial->bitmap, i))
                    log_warning(logger, "Se intento ocupar el bloque %u pero ya se encontraba ocupado.");
                else
                    log_debug(logger, "Se pudo ocupar el bloque %u correctamente, faltan %u.", i, pos_arranque + bloques_a_agregar - i);
                // limpio el bloque del archivo asi no tiene basura
                limpiar_bloque(idial->bloques, i);
            }
            log_debug(logger, "Termine de truncar. Voy a Modificar el archivo de metadata.");

            // seteo la metadata (solamente varia el size)
            config_set_value(config, "TAMANIO_ARCHIVO", string_itoa(nuevo_size));
            config_save(config);
            resultado = 1;
        }
        else if (hay_espacio_suficiente(idial, bloques_a_agregar))
        {
            log_info(logger, "PID: <%u> - Inicio Compactación.", pid);
            compactar(idial, nombre_archivo, nuevo_size);

            log_info(logger, "PID: <%u> - Fin Compactación.", pid);
            resultado = 1;
        }
        else
        {
            log_error(logger, "Es imposible truncar el archivo %s para el proceso %u, ya que no hay espacio suficiente.", nombre_archivo, pid);
        }

    } // si es menor no valido nada, interpreto que es valido perder informacion al truncar
    else
    {
        uint32_t cant_bloques_actual = (uint32_t)ceil( (double)size_archivo / (double) idial->block_size);
        uint32_t cant_bloques_nueva = (uint32_t)ceil((double) nuevo_size /(double) idial->block_size);
        uint32_t bloques_a_sacar = cant_bloques_actual - cant_bloques_nueva;
        uint32_t pos_arranque = bloque_inicial + size_archivo;

        // siempre voy a tener espacio para truncar ya que estoy borrando
        for (uint32_t i = pos_arranque; i > pos_arranque - bloques_a_sacar; i--)
        {
            if (!liberar_bloque(idial->bitmap, i))
                log_warning(logger, "Se intento liberar el bloque %u pero ya se encontraba libre.");
            else
                log_debug(logger, "Se pudo liberar el bloque %u correctamente, faltan %u.", i, i - (pos_arranque - bloques_a_sacar));
        }

        // seteo la metadata (solamente varia el size)
        config_set_value(config, "TAMANIO_ARCHIVO", nuevo_size);
        config_save(config);
        resultado = 1;
    }

    // cierro todo
    config_destroy(config);
    free(path_metadata);

    return resultado;
}

uint8_t hay_espacio_suficiente(t_interfaz_dialfs *idial, uint32_t nuevo_cant_bloques)
{
    uint32_t contador = 0;
    for (uint32_t i = 0; i < idial->block_count; i++)
    {
        if (!esta_bloque_ocupado(idial->bitmap, i))
            contador++;
    }
    return nuevo_cant_bloques <= contador;
}

uint8_t escribir_en_archivo(t_interfaz_dialfs *idialfs, char *nombre_archivo, uint32_t size_dato, uint32_t puntero_archivo, void *dato)
{
    // consigo la info del archivo
    char *path_metadata = armar_path_metadata(nombre_archivo, idialfs->path_base_dialfs);
    t_config *config = config_create(path_metadata);
    uint32_t bloque_inicial = (uint32_t)config_get_int_value(config, "BLOQUE_INICIAL");
    uint32_t size_archivo = (uint32_t)config_get_int_value(config, "TAMANIO_ARCHIVO");
    uint32_t puntero_en_disco = bloque_inicial * idialfs->block_size + puntero_archivo;

    log_debug(logger, "PeD: %u | BI: %u | PA: %u", puntero_en_disco, bloque_inicial, puntero_archivo);

    // puedo escribir??? => valido q tengo el espacio suficiente
    if (puntero_en_disco + size_dato > bloque_inicial * idialfs->block_size + size_archivo)
        return 0;

    escribir_bloque(idialfs->bloques, puntero_en_disco, size_dato, dato);
    free(path_metadata); // esto no estaba puesto, despues ver si no rompe nada 9/7/24
    config_destroy(config);

    return 1;
}

void *leer_en_archivo(t_interfaz_dialfs *idialfs, char *nombre_archivo, uint32_t size_dato, uint32_t puntero_archivo)
{
    // consigo la info del archivo
    char *path_metadata = armar_path_metadata(nombre_archivo, idialfs->path_base_dialfs);
    t_config *config = config_create(path_metadata);
    uint32_t bloque_inicial = (uint32_t)config_get_int_value(config, "BLOQUE_INICIAL");
    uint32_t size_archivo = (uint32_t)config_get_int_value(config, "TAMANIO_ARCHIVO");
    uint32_t puntero_en_disco = (bloque_inicial * idialfs->block_size) + puntero_archivo;

    config_destroy(config); // esto no estaba puesto, despues ver si no rompe nada 9/7/24
    free(path_metadata); // esto no estaba puesto, despues ver si no rompe nada 9/7/24

    if (puntero_en_disco + size_dato > bloque_inicial * idialfs->block_size + size_archivo)
        return 0;

    return leer_bloque(idialfs->bloques, puntero_en_disco, size_archivo);
}

char *armar_path_metadata(char *nombre_archivo, char *path)
{
    char *path_metadata = string_duplicate(path);
    string_append(&path_metadata, nombre_archivo); // /dialfs/algo/nombre_archivo
    string_append(&path_metadata, ".metadata");    // /dialfs/algo/nombre_archivo.txt
    return path_metadata;
}

void compactar(t_interfaz_dialfs *idialfs, char *archivo_agrandar, uint32_t nuevo_size)
{
    // consigo la info del archivo
    char *path_metadata = armar_path_metadata(archivo_agrandar, idialfs->path_base_dialfs);
    t_config *config = config_create(path_metadata);
    uint32_t bloque_inicial = (uint32_t)config_get_int_value(config, "BLOQUE_INICIAL");
    uint32_t size_archivo = (uint32_t)config_get_int_value(config, "TAMANIO_ARCHIVO");
    uint32_t cant_bloques = (uint32_t)ceil((double)size_archivo / (double)idialfs->block_size); // ceil redondea para arriba

    // caso especial SOLAMENTE si el el truncate es para un archivo recien creado (size 0)
    if (size_archivo == 0)
    { // esto es xq aunque yo en el bitmap le asigno un bloque al archivo cuando lo creo, en la metadata le tengo que poner tamanio 0
        cant_bloques++;
    }

    log_debug(logger, "Bloque inicial archivo_agrandar: %u", bloque_inicial);
    // primero voy a sacar el archivo q quiere agrandarse
    void *buffer = leer_en_archivo(idialfs, archivo_agrandar, size_archivo, 0);
    mem_hexdump(buffer, size_archivo);
    limpiar_bitmap(idialfs, bloque_inicial, cant_bloques);

    // desp ejecuto el algoritmo para mover el resto de archivos hacia la izquierda
    uint32_t ultimo_bloque_libre = aplicar_algoritmo_compactacion(idialfs);
    log_debug(logger, "Mi archivo ahora va a arrancar en %u", ultimo_bloque_libre);

    usleep(idialfs->retraso_compactacion * 1000);

    // meto al final al archivo q saque
    config_set_value(config, "BLOQUE_INICIAL", string_itoa((int)ultimo_bloque_libre));
    uint32_t puntero_en_disco = (ultimo_bloque_libre * idialfs->block_size);
    config_set_value(config, "TAMANIO_ARCHIVO", string_itoa((int)nuevo_size));
    ocupar_bitmap(idialfs, ultimo_bloque_libre, (uint32_t)ceil((double)nuevo_size / (double)idialfs->block_size));
    escribir_bloque(idialfs->bloques, puntero_en_disco, size_archivo, buffer);
    log_debug(logger, "Se re-escribio el archivo a partir del bloque %u, con size %u", puntero_en_disco, size_archivo);
    config_save(config);
    // lloro :´( 😭
}

void limpiar_bitmap(t_interfaz_dialfs *idial, uint32_t bloque_inicial, uint32_t cant_bloques)
{
    for (uint32_t i = bloque_inicial; i < bloque_inicial + cant_bloques; i++)
    {
        if (!liberar_bloque(idial->bitmap, i)) // solamente lo printeo xq en si no es algo que me pueda dar un error
            log_warning(logger, "El bloque ya se encontraba liberado");
        else
            log_info(logger, "Se libero el bloque");
    }
}

void ocupar_bitmap(t_interfaz_dialfs *idial, uint32_t bloque_inicial, uint32_t cant_bloques)
{
    for (uint32_t i = bloque_inicial; i < bloque_inicial + cant_bloques; i++)
    {
        if (!ocupar_bloque(idial->bitmap, i)) // solamente lo printeo xq en si no es algo que me pueda dar un error
            log_warning(logger, "El bloque ya se encontraba ocupado (-_-)");
        else
        {
            log_trace(logger, "Se ocupo el bloque");
            limpiar_bloque(idial->bloques, i);
        }
    }
}

uint32_t aplicar_algoritmo_compactacion(t_interfaz_dialfs *idial)
{
    t_config *config_archivo_a_mover;
    uint32_t primer_bloque_libre = 0;
    uint8_t busco_bloque_libre = true;

    for (int i = 0; i < idial->block_count; i++)
    {
        if (!esta_bloque_ocupado(idial->bitmap, i) && busco_bloque_libre)
        {
            log_debug(logger, "Bloque libre en %d", i);
            primer_bloque_libre = i;
            busco_bloque_libre = false;
        }
        if (esta_bloque_ocupado(idial->bitmap, i) && !busco_bloque_libre)
        {
            log_debug(logger, "Bloque ocupado en %d", i);
            config_archivo_a_mover = conseguir_config_archivo_por_inicio(idial, i);
            mover_archivo(idial, primer_bloque_libre, config_archivo_a_mover);
            uint32_t bloque_inicial = (uint32_t)config_get_int_value(config_archivo_a_mover, "BLOQUE_INICIAL");
            uint32_t tamanio_archivo = (uint32_t)config_get_int_value(config_archivo_a_mover, "TAMANIO_ARCHIVO");
            uint32_t cant_bloques = (uint32_t)ceil((double)tamanio_archivo / (double)idial->block_size); // ceil redondea para arriba
            if(tamanio_archivo == 0)
            {
                cant_bloques++;
            }
            primer_bloque_libre = bloque_inicial + cant_bloques;
            i = primer_bloque_libre - 1; // primer_bloque_libre = primer_bloque_libre-1
            
        }
    }
    config_destroy(config_archivo_a_mover);
    return primer_bloque_libre;
}

t_config *conseguir_config_archivo_por_inicio(t_interfaz_dialfs *idialfs, int i)
{
    t_config *config_archivo;
    DIR *d;
    struct dirent *directorio;
    char *base = string_duplicate("./");
    string_append(&base, (idialfs->path_base_dialfs));
    base = string_substring(base, 0, string_length(base) - 1);
    log_debug(logger, "PATH: %s", base);
    d = opendir(base);
    if (d == NULL)
    {
        log_error(logger, "No se pudo abrir el directorio.");
        return EXIT_FAILURE;
    }

    bool lo_encontre = false;

    while ((directorio = readdir(d)) && !lo_encontre)
    {
        if (string_ends_with(directorio->d_name, ".metadata"))
        {
            char *path_completo = string_duplicate(idialfs->path_base_dialfs);
            string_append(&path_completo, directorio->d_name);
            log_debug(logger, "Voy a ver si el archivo con el path %s es el que necesito", path_completo);
            config_archivo = config_create(path_completo);
            uint32_t bloque_inicial = (uint32_t)config_get_int_value(config_archivo, "BLOQUE_INICIAL");
            if (bloque_inicial == i)
            {
                log_debug(logger, "El archivo con el inicio %d es el %s", i, path_completo);
                lo_encontre = true;
            }
            free(path_completo);
        }
    }
    closedir(d);
    free(base);
    return config_archivo;
}

void mover_archivo(t_interfaz_dialfs *idialfs, int nuevo_origen, t_config *config_archivo)
{
    log_debug(logger, "Voy a mover a un archivo su bloque inicial a %d", nuevo_origen);
    uint32_t bloque_inicial = (uint32_t)config_get_int_value(config_archivo, "BLOQUE_INICIAL");
    uint32_t size_archivo = (uint32_t)config_get_int_value(config_archivo, "TAMANIO_ARCHIVO");
    uint32_t puntero_en_disco = (bloque_inicial * idialfs->block_size);
    uint32_t cant_bloques = (uint32_t)ceil( (double) size_archivo / (double) idialfs->block_size); // ceil redondea para arriba
    
    // caso especial SOLAMENTE si el el truncate es para un archivo recien creado (size 0)
    if (size_archivo == 0)
    { 
        cant_bloques++;
    }
    uint32_t actual_ultimo_bloque = bloque_inicial + cant_bloques - 1;          // 1 
    uint32_t nuevo_ultimo_bloque = nuevo_origen + cant_bloques - 1;             // 0
    uint32_t desplazamiento = bloque_inicial - nuevo_origen;                    // 1 - 0 = 1
    void *informacion_archivo = leer_bloque(idialfs->bloques, puntero_en_disco, size_archivo);

    config_set_value(config_archivo, "BLOQUE_INICIAL", string_itoa(nuevo_origen));
    config_save(config_archivo);
    puntero_en_disco = (nuevo_origen * idialfs->block_size);
    ocupar_bitmap(idialfs, nuevo_origen, desplazamiento);
    escribir_bloque(idialfs->bloques, puntero_en_disco, size_archivo, informacion_archivo);
    limpiar_bitmap(idialfs, nuevo_ultimo_bloque + 1, desplazamiento);
}

void destruir_interfaz(t_interfaz_default* interfaz) // esto no estaba puesto, despues ver si no rompe nada 10/7/24
{
    switch (interfaz->tipo_interfaz)
    {
    case STDIN:
        t_interfaz_stdin* istdin = (t_interfaz_stdin*)interfaz->configs_especificas;
        liberar_conexion(istdin->conexion_memoria, logger);
        break;
    case STDOUT:
        t_interfaz_stdout* istdout = (t_interfaz_stdout*)interfaz->configs_especificas;
        liberar_conexion(istdout->conexion_memoria, logger);
        break;
    case DIALFS:
        t_interfaz_dialfs* idialfs = (t_interfaz_dialfs*)interfaz->configs_especificas;
        liberar_conexion(idialfs->conexion_memoria, logger);
        free(idialfs->bitmap->path);
        bitarray_destroy(idialfs->bitmap->bitarray);
        free(idialfs->bitmap);
        free(idialfs->bloques->path);
        munmap(idialfs->bloques->bloques, (idialfs->bloques->cant_bloques)*(idialfs->bloques->size_bloque));
        free(idialfs->bloques);
        free(idialfs->path_base_dialfs);
        break;
    default:
        break;
    }
    liberar_conexion(interfaz->conexion_kernel, logger);
    free(interfaz->configs_especificas);
    free(interfaz->nombre);
    free(interfaz);
}