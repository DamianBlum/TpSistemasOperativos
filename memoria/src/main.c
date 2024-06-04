#include "main.h"

t_log *logger;
t_config *config;

// servidor
int socket_servidor;

// clientes
int cliente_kernel;
int cliente_cpu;

// hilos
pthread_t tid[3];
FILE *archivo_text_proceso;
// diccionario de procesos
t_dictionary *procesos;

// path de los archivos
char *path;

// Variable tam de pag
int tam_pag;
// Variable tam memoria
int tam_memoria;
// Variable espacio memoria
void *espacio_memoria;
// Varibale Marcos Libres
t_bitarray *marcos;
// Varibale cantidad marcos
int cant_marcos;

int main(int argc, char *argv[])
{
    // hago prueba de creacion de la memoria y de la creacion de un proceso con sus respectivas tablas
    procesos = dictionary_create();
    logger = iniciar_logger("memoria.log", "MEMORIA", argc, argv);
    config = iniciar_config("memoria.config");
    tam_pag = config_get_int_value(config, "TAM_PAGINA");
    path = config_get_string_value(config, "PATH_INSTRUCCIONES");
    crear_espacio_memoria();
    crear_proceso("pk", 1);
    //crear_proceso("pk", 2);
    t_memoria_proceso* proceso1 = encontrar_proceso(1);
    //t_memoria_proceso* proceso2 = encontrar_proceso(2);
    agregar_paginas(proceso1,0,4);
    
    hacer_pedido_escritura(0,14,1,"holahijodeputa",0);  
    char* resultado=(char*)hacer_pedido_lectura(0,14,1,0);
    log_info(logger,"Lo que lei fue esta palabra: %s",resultado);
    destruir_proceso(1);
    free(espacio_memoria);
    //list_destroy(lista2);
    // HAy que ver como hacer para pasar a la siguiente pagina ya que no la tenemos y el indice es la pagina en la tabla!
    /*
    procesos = dictionary_create();
    logger = iniciar_logger("memoria.log", "MEMORIA", argc, argv);
    config = iniciar_config("memoria.config");
    tam_pag = config_get_int_value(config, "TAM_PAGINA");
    path = config_get_string_value(config, "PATH_INSTRUCCIONES");
    crear_espacio_memoria();

    socket_servidor = iniciar_servidor(config, "PUERTO_ESCUCHA");

    log_info(logger, "Servidor %d creado.", socket_servidor);

    // 3 clientes - CPU - I/O - KERNEL
    // Sabemos que los 3 clientes se conectan en orden: CPU => KERNEL => I/O

    // Recibimos la conexion de la cpu y creamos un hilo para trabajarlo
    cliente_cpu = esperar_cliente(socket_servidor, logger);
    pthread_create(&tid[SOY_CPU], NULL, servidor_cpu, NULL);

    // Recibimos la conexion de la kernel y creamos un hilo para trabajarlo
    cliente_kernel = esperar_cliente(socket_servidor, logger);
    pthread_create(&tid[SOY_KERNEL], NULL, servidor_kernel, NULL);

    // Recibimos las multiples conexiones de la I/O y creamos los hilo para trabajar
    pthread_create(&tid[SOY_IO], NULL, esperar_io, NULL);

    pthread_join(tid[SOY_CPU], NULL);
    pthread_join(tid[SOY_KERNEL], NULL);
    */
    destruir_logger(logger);
    destruir_config(config);
    printf("Memoria finalizada\n");
    return 0;
}

void *esperar_io(void *arg)
{
    while (1)
    {
        // con la variable pthread_t *hilos_io, guardo la direccion de memoria de los hilos que voy creando
        pthread_t hilo_io;
        int cliente_entradasalida = esperar_cliente(socket_servidor, logger);
        pthread_create(&hilo_io, NULL, servidor_entradasalida, cliente_entradasalida);
    }
}

void *servidor_kernel(void *arg)
{
    log_info(logger, "El socket cliente %d entro al servidor de Kernel", cliente_kernel);

    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(cliente_kernel, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(cliente_kernel, logger);
            log_info(logger, "Desde cliente %d: Recibi el mensaje: %s.", cliente_kernel, mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(cliente_kernel, logger);
            log_info(logger, "Desde cliente %d: Recibi un paquete.", cliente_kernel);
            e_operacion operacion_kernel = (e_operacion)list_get(lista, 0);
            sleep(config_get_int_value(config, "RETARDO_RESPUESTA") / 1000);
            switch (operacion_kernel)
            {

            case INICIAR_PROCESO:
                char *nombre_archivo = list_get(lista, 1);
                uint32_t process_id = (uint32_t)list_get(lista, 2);
                int resultado = crear_proceso(nombre_archivo, process_id);

                if (resultado == 0)
                {
                    log_info(logger, "Se creo el proceso %d con el archivo %s.", process_id, nombre_archivo);
                }
                else
                {
                    log_error(logger, "No se pudo crear el proceso %d con el archivo %s.", process_id, nombre_archivo);
                }

                t_paquete *paquete_a_enviar = crear_paquete();
                agregar_a_paquete(paquete_a_enviar, resultado, sizeof(int));
                enviar_paquete(paquete_a_enviar, cliente_kernel, logger);
                eliminar_paquete(paquete_a_enviar); //LO TENIA COMENTADO, PROBAR DESPUES CON KERNEL
                log_debug(logger, "Envie el resultado de la operacion INICIAR_PROCESO al kernel");
                break;
            case BORRAR_PROCESO:
                uint32_t pid = (uint32_t)list_get(lista, 1);
                destruir_proceso(pid);
                t_paquete *paquete_a_enviar = crear_paquete();
                agregar_a_paquete(paquete_a_enviar, 0, sizeof(int));
                enviar_paquete(paquete_a_enviar, cliente_kernel, logger);
                eliminar_paquete(paquete_a_enviar);
                break;
            default:
                break;
            }

            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", cliente_cpu);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente %d: Recibi una operacion rara (%d), termino el servidor.", cliente_cpu, operacion);
            return EXIT_FAILURE;
            break;
        }
    }
    return EXIT_SUCCESS;
}

void *servidor_cpu(void *arg)
{
    log_info(logger, "El socket cliente %d entro al servidor de CPU", cliente_cpu);
    // abro el archivo txt "prueba.txt"para leerlo como pruebo de la comunicacion
    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(cliente_cpu, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(cliente_cpu, logger);
            log_info(logger, "Desde cliente %d:Recibi el mensaje: %s.", cliente_cpu, mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(cliente_cpu, logger);
            log_info(logger, "Desde cliente: %d Recibi un paquete.", cliente_cpu);

            // aplico el retardo de la respuesta
            sleep(config_get_int_value(config, "RETARDO_RESPUESTA") / 1000);

            e_operacion operacion_memoria = (e_operacion)list_get(lista,0);
            switch (operacion_memoria)
            {
            case PEDIDO_LECTURA:
                uint32_t df = (uint32_t)list_get(lista, 1);
                uint32_t size = (uint32_t)list_get(lista, 2);
                uint32_t pid = (uint32_t)list_get(lista, 3);
                uint8_t tipo = (uint8_t)list_get(lista, 4);

                void* resultado=hacer_pedido_lectura(df, size, pid, tipo);
                if (tipo){
                    uint32_t valor = (uint32_t)resultado;
                    t_paquete *paquete_a_enviar = crear_paquete();
                    agregar_a_paquete(paquete_a_enviar, valor, size);
                    enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
                    eliminar_paquete(paquete_a_enviar);
                }
                else{
                    char* texto = (char*)resultado;
                    t_paquete *paquete_a_enviar = crear_paquete();
                    agregar_a_paquete(paquete_a_enviar, texto, size);
                    enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
                    eliminar_paquete(paquete_a_enviar);
                }
                break;
            case PEDIDO_ESCRITURA:
                uint32_t df = (uint32_t)list_get(lista, 1);
                uint32_t size = (uint32_t)list_get(lista, 2);
                uint32_t pid = (uint32_t)list_get(lista, 3);
                void *elemento_a_insertar = list_get(lista, 4);
                uint8_t tipo = (uint8_t)list_get(lista, 5);
                int resultado=hacer_pedido_escritura(df, size, pid, elemento_a_insertar, tipo);
                if (resultado == 0)
                {
                    enviar_mensaje("OK", cliente_cpu, logger);
                }
                break;
            case OBTENER_MARCO:
                uint32_t pid = (uint32_t)list_get(lista, 1);
                uint32_t pagina = (uint32_t)list_get(lista, 2);
                obtener_marco(pid, pagina);
                break;
            case OBTENER_INSTRUCCION:
                obtener_instruccion(lista);
                break;
            case MODIFICAR_TAMANIO_PROCESO:
                modificar_tamanio_proceso(lista);
                break;
            default: // recibi algo q no es eso, vamos a suponer q es para terminar
                log_error(logger, "Desde cliente: %d Recibi una operacion de memoria rara (%d), termino el servidor.", operacion_memoria);
                return EXIT_FAILURE;
            }
            break;
        case EXIT: // indica desconexion
            log_error(logger, "Se desconecto el cliente %d.", cliente_cpu);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente: %d Recibi una operacion rara (%d), termino el servidor.", operacion);
            return EXIT_FAILURE;
        
        }
    }
    return EXIT_SUCCESS;
}

void *servidor_entradasalida(void *arg)
{
    int cliente_entradasalida = (int)arg;
    log_info(logger, "El socket cliente %d entro al servidor de I/O", cliente_entradasalida);

    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(cliente_entradasalida, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(cliente_entradasalida, logger);
            log_info(logger, "Desde cliente %d: Recibi el mensaje: %s.", cliente_entradasalida, mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(cliente_entradasalida, logger);
            log_info(logger, "Desde cliente %d: Recibi un paquete.", cliente_entradasalida);
            e_operacion operacion_io = (e_operacion)list_get(lista, 0);
            sleep(config_get_int_value(config, "RETARDO_RESPUESTA")/1000);
            switch (operacion_io)
            {
                case PEDIDO_ESCRITURA:
                    uint32_t df = (uint32_t)list_get(lista, 1);
                    uint32_t size = (uint32_t)list_get(lista, 2);
                    uint32_t pid = (uint32_t)list_get(lista, 3);
                    void *elemento_a_insertar = list_get(lista, 4);
                    uint8_t tipo = (uint8_t)list_get(lista, 5);
                    int resultado=hacer_pedido_escritura(df, size, pid, elemento_a_insertar, tipo);
                    if (resultado == 0)
                    {
                        enviar_mensaje("OK", cliente_entradasalida, logger);
                    }
                    break;
                case PEDIDO_LECTURA:
                    uint32_t df = (uint32_t)list_get(lista, 1);
                    uint32_t size = (uint32_t)list_get(lista, 2);
                    uint32_t pid = (uint32_t)list_get(lista, 3);
                    uint8_t tipo = (uint8_t)list_get(lista, 4);

                    void* resultado=hacer_pedido_lectura(df, size, pid, tipo);
                    if (tipo){
                        uint32_t valor = (uint32_t)resultado;
                        t_paquete *paquete_a_enviar = crear_paquete();
                        agregar_a_paquete(paquete_a_enviar, valor, size);
                        enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
                        eliminar_paquete(paquete_a_enviar);
                    }
                    else{
                        char* texto = (char*)resultado;
                        t_paquete *paquete_a_enviar = crear_paquete();
                        agregar_a_paquete(paquete_a_enviar, texto, size);
                        enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
                        eliminar_paquete(paquete_a_enviar);
                    }
                    break;
            }
            break;
        case EXIT: // indica desconexion
            log_error(logger, "Se desconecto el cliente %d.", cliente_entradasalida, cliente_entradasalida);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente %d: Recibi una operacion rara (%d), termino el servidor.", cliente_entradasalida, operacion);
        }
    }
    return EXIT_SUCCESS;
}

// escribe en memoria en Little Endian (de derecha a izquierda)

void* hacer_pedido_lectura(uint32_t df, uint32_t size, uint32_t pid, uint8_t tipo){
    uint32_t size_original = size;
    char * elemento_a_guardar = malloc(size_original);
    void * resultado;
    uint32_t marco = floor(df / tam_pag);
    uint32_t cuanto_voy_guardando = 0;
    while (size) {
        uint32_t memoria_disponible  = ((marco + 1) * tam_pag - df);
        if (memoria_disponible >= size) {
            memcpy(elemento_a_guardar+cuanto_voy_guardando, espacio_memoria + df, size);
            size = 0;
        }
        else {  
            memcpy(elemento_a_guardar+cuanto_voy_guardando, espacio_memoria + df, memoria_disponible);
            size -= memoria_disponible;
            cuanto_voy_guardando += memoria_disponible; 
            marco = conseguir_siguiente_marco(pid,marco);
            df = marco * tam_pag;
        }
    }
    if (tipo)
    {
        uint8_t* direccion = (uint8_t*)elemento_a_guardar;
        uint32_t valor = 0;
        for (int i = 0; i < size_original; i++) {
            valor |= ((uint32_t)direccion[i] << (i * 8));
        }
        printf("Valor leído: %u\n", valor);
        resultado = malloc(sizeof(uint32_t));
        resultado = (void*)valor;

    }
    else {
        resultado=(void*)string_substring_until(elemento_a_guardar,size_original);
        log_info(logger,"Lo que lei fue este texto: %s",resultado);
    }


    return resultado;
}

int hacer_pedido_escritura(uint32_t df, uint32_t size, uint32_t pid,void *elemento_a_insertar,uint8_t tipo){
        resultado = -1; 
        uint32_t marco = floor(df / tam_pag);
        log_debug(logger, "Marco: %d", marco);
        log_info(logger,"PID: <%u> - Accion: <ESCRIBIR> - Direccion fisica: %u - Tamaño <%u>", pid, df, size);
        
        while (size) {
            uint32_t memoria_disponible  = ((marco + 1) * tam_pag - df);
            log_debug(logger, "Memoria disponible: %d", memoria_disponible);
            log_debug(logger, "Tamaño que tengo que seguir escribiendo: %d", size);
            if (memoria_disponible >= size) {
                if (tipo)
                {
                    memcpy(espacio_memoria + df, &elemento_a_insertar, size);
                    resultado = 0;
                }
                else
                {
                    log_debug(logger, "Elemento a insertar: %s", elemento_a_insertar);
                    memcpy(espacio_memoria + df, elemento_a_insertar, size);
                    resultado = 0;
                }
                size = 0;
            }
            else {  
                if (tipo)
                {
                    memcpy(espacio_memoria + df, &elemento_a_insertar, memoria_disponible);
                    size -= memoria_disponible;
                    elemento_a_insertar = ((uint32_t) elemento_a_insertar >> 8*memoria_disponible) & 0xFFFFFFFF;
                }
                else
                {
                    memcpy(espacio_memoria + df, elemento_a_insertar, memoria_disponible);
                    size -= memoria_disponible;
                    elemento_a_insertar += memoria_disponible;
                }
                marco = conseguir_siguiente_marco(pid,marco);
                log_debug(logger, "marco: %u", marco);
                df = marco * tam_pag;
            }
        }
        mem_hexdump(espacio_memoria, tam_memoria);
        return resultado;
}

void obtener_instruccion(t_list* lista){
    uint32_t pid = (uint32_t)list_get(lista, 1);
    uint32_t pc = (uint32_t)list_get(lista, 2);
    log_debug(logger, "El pid es: %d y el pc es: %d", pid, pc);
    t_memoria_proceso *proceso_actual = encontrar_proceso(pid);
    char *linea_de_instruccion = string_duplicate(proceso_actual->lineas_de_codigo[pc]);
    log_debug(logger, "linea de codigo: %s", linea_de_instruccion);
    enviar_mensaje(linea_de_instruccion, cliente_cpu, logger);
    free(linea_de_instruccion);
}

int crear_proceso(char *nombre_archivo, uint32_t process_id)
{
    char **lineas = string_array_new();
    char *linea, longitud[100];

    log_debug(logger, "Voy a crear el proceso %d con el archivo %s", process_id, nombre_archivo);

    char *path_completo = string_from_format("%s%s", path, nombre_archivo);

    log_debug(logger, "El path del archivo es: %s", path_completo);

    // abro el archivo que queda en la carpeta de la memoria que saco del config en PATH_INSTRUCCIONES con nombre de archivo nombre_archivo
    archivo_text_proceso = fopen(path_completo, "r");

    // leo cada linea y la imprimo por pantalla
    if (archivo_text_proceso == NULL)
    {
        log_error(logger, "Error al abrir el archivo");
        return 1;
    }

    while (fgets(longitud, 100, archivo_text_proceso) != NULL)
    {
        linea = string_duplicate(longitud);
        // linea se esta guardando al final con un \n y despues con el \O, ahora elimino el \n pero dejando el \0
        if (linea[strlen(linea) - 1] == '\n')
            linea[strlen(linea) - 1] = '\0';
        log_debug(logger, "Linea: %s", linea);
        string_array_push(&lineas, linea);
    }

    fclose(archivo_text_proceso);

    t_memoria_proceso *proceso = crear_estructura_proceso(nombre_archivo, lineas);

    log_info(logger, "PID: <%u> - Tamaño: <0>\n", process_id);
    // lo guardo en mi diccionario de procesos
    //  paso de uint32_t a string
    char *string_pid = string_itoa((int)process_id);

    dictionary_put(procesos, string_pid, proceso);

    // libero variables
    free(path_completo);
    free(string_pid);

    return 0;
}

t_memoria_proceso *crear_estructura_proceso(char *nombre_archivo, char **lineas_de_codigo)
{
    // Creo un proceso con el nombre del archivo y el vector posiciones
    t_memoria_proceso *proceso = malloc(sizeof(t_memoria_proceso));
    proceso->nombre_archivo = string_duplicate(nombre_archivo);
    proceso->tabla_paginas = malloc(sizeof(uint32_t)*(tam_memoria/tam_pag));
    proceso->lineas_de_codigo = lineas_de_codigo;
    proceso->paginas_actuales = 0;
    return proceso;
}

t_memoria_proceso *encontrar_proceso(uint32_t pid)
{
    log_trace(logger, "Buscando proceso %d", pid);
    // busco el proceso en el diccionario de procesos
    char *string_pid = string_itoa((int)pid);
    t_memoria_proceso *proceso = dictionary_get(procesos, string_pid);
    log_debug(logger, "Proceso encontrado %s", proceso->nombre_archivo);
    free(string_pid);
    return proceso;
}

void destruir_proceso(uint32_t pid)
{
    // busco el proceso en el diccionario de procesos
    char *string_pid = string_itoa((int)pid);
    t_memoria_proceso *proceso = dictionary_remove(procesos, string_pid);
    borrar_paginas(proceso, 0, proceso->paginas_actuales);
    // libero la memoria del proceso
    free(proceso->nombre_archivo);
    string_array_destroy(proceso->lineas_de_codigo);
    free(proceso->tabla_paginas);
    free(proceso);
    free(string_pid);
}

void crear_espacio_memoria()
{
    tam_memoria = config_get_int_value(config, "TAM_MEMORIA"); // 64 bytes
    cant_marcos = tam_memoria / tam_pag; // tam_pag = 8 bytes => 8 marcos
    espacio_memoria = malloc(tam_memoria);
    int bytes_redondeados = ceil(cant_marcos / 8);
    void *puntero_a_bits = malloc(bytes_redondeados);
    marcos = bitarray_create_with_mode(puntero_a_bits, bytes_redondeados, MSB_FIRST);

    // hago prueba, recorro todo el bitmap y voy mostrando su contenido
    for (int i = 0; i < cant_marcos; i++)
    {
        bitarray_clean_bit(marcos, i);
    }
}

uint32_t devolver_marco(uint32_t pid, uint32_t pagina)
{

    t_memoria_proceso *proceso = encontrar_proceso(pid);
    uint32_t marco = proceso->tabla_paginas[pagina];
    log_info(logger, "PID: <%u> - Pagina: <%u> - Marco: <%u>\n", pid, pagina, marco);
    return marco;
}

uint32_t moverme_a_marco(uint32_t marco)
{
    uint32_t inicio_marco = marco * tam_pag ;
    return inicio_marco;
}


uint32_t conseguir_siguiente_marco(uint32_t pid,uint32_t marco)
{
    t_memoria_proceso *proceso = encontrar_proceso(pid);
    // tengo que recorrer todos los marcos asignados a mi proceso hasta encontrar el que tengo y luego, devolver el siguiente
    uint32_t sig_marco;
    for (int i = 0; i < proceso->paginas_actuales; i++)
    {
        if (proceso->tabla_paginas[i] == marco)
        {
            sig_marco = proceso->tabla_paginas[i + 1];
            log_info(logger, "PID: <%u> - Marco actual: <%u> - Siguiente Marco: <%u>", pid, marco, sig_marco);
        }
    }
    return sig_marco;
}


void borrar_paginas(t_memoria_proceso* proceso,uint32_t paginas_necesarias,uint32_t paginas_actuales)
{   
    // Empieza en la ultima tabla y las va popeando, esto permite que se realoque :p
    for (int i = paginas_actuales; i > paginas_necesarias; i--)
    {
        // modifico el bitmap marcos para que el marco vuelva a estar libre
        bitarray_clean_bit(marcos, proceso->tabla_paginas[i]);
        proceso->tabla_paginas[i]=0;
        proceso->paginas_actuales--;
    }
}

int agregar_paginas(t_memoria_proceso* proceso,uint32_t paginas_actuales,uint32_t paginas_necesarias)
{
    for (int i = paginas_actuales; i < paginas_necesarias; i++)
    {
        // busco un marco libre
        int marco_libre = bitarray_buscar_libre(cant_marcos);
        log_debug(logger, "Marco libre: %d", marco_libre);
        if (marco_libre == -1)
        {
            log_error(logger, "Out Of Memory");
            return -1;
        }
        
        // modifico el bitmap marcos para que el marco no este libre
        bitarray_set_bit(marcos, marco_libre);
        // agrego el marco a la tabla de paginas
        proceso->tabla_paginas[i] = marco_libre;
        proceso->paginas_actuales++;
        log_debug(logger, "Marco agregado: %u en la pagina %u", proceso->tabla_paginas[i], i);
    }
    return EXIT_SUCCESS;
}


int bitarray_buscar_libre(int cant_marcos)
{
    for (int i = 0; i < cant_marcos; i++)
    {
        if (bitarray_test_bit(marcos, i) == 0)
        {
            return i;
        }
    }
    return -1;
}

void obtener_marco(uint32_t pid, uint32_t pagina)
{
    uint32_t marco = devolver_marco(pid, pagina);
    t_paquete *paquete_a_enviar = crear_paquete();
    agregar_a_paquete(paquete_a_enviar, marco, sizeof(uint32_t));
    enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
    eliminar_paquete(paquete_a_enviar);
}

void modificar_tamanio_proceso(t_list* lista) 
{
    int resultado = 0;
    // conseguir el nuevo tamaño del proceso
    uint32_t nuevo_tam = (uint32_t)list_get(lista, 1); //es en bytes
    // nuevo tam divido tam de pag para saber cuantas paginas necesito
    // si no las tengas voy desde la primera que necesito en adelante
    uint32_t pid = (uint32_t)list_get(lista, 2);
    t_memoria_proceso *proceso = encontrar_proceso(pid);
    uint32_t paginas_necesarias = nuevo_tam / (uint32_t)tam_pag;
    uint32_t paginas_actuales = (uint32_t)string_length(proceso->tabla_paginas);
    if (paginas_actuales>paginas_necesarias)
    {
        borrar_paginas(proceso, paginas_necesarias, paginas_actuales);
    }
    else if (paginas_actuales<paginas_necesarias)
    {
        resultado = agregar_paginas(proceso, paginas_actuales, paginas_necesarias);
    }
    t_paquete *paquete_a_enviar = crear_paquete();
    agregar_a_paquete(paquete_a_enviar, resultado, sizeof(int));
    enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
    eliminar_paquete(paquete_a_enviar);
}


