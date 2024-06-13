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

//archivo para abrir los pseudocodigos
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
    inicializar_modulo_memoria(argc, argv);
    //testear_modulo_memoria();
    creacion_servidor_y_clientes();
    finalizar_modulo_memoria();
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
                log_debug(logger, "Envie el resultado de la operacion INICIAR_PROCESO al kernel");
                break;
            case BORRAR_PROCESO:
                uint32_t pid = (uint32_t)list_get(lista, 1);
                destruir_proceso(pid);
                t_paquete *paquete = crear_paquete();
                agregar_a_paquete(paquete, 0, sizeof(int));
                enviar_paquete(paquete, cliente_kernel, logger);
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
                uint8_t tipo = (uint8_t)list_get(lista, 4);
                uint32_t size = (uint32_t)list_get(lista, 2);
                void* resultado_lectura=hacer_pedido_lectura(lista);
                if (tipo){
                    uint32_t valor = *(uint32_t*)resultado_lectura; 
                    t_paquete *paquete_a_enviar = crear_paquete();
                    agregar_a_paquete(paquete_a_enviar, valor, size);
                    enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
                }
                else{
                    char* texto = string_duplicate((char*)resultado_lectura);
                    t_paquete *paquete_a_enviar = crear_paquete();
                    agregar_a_paquete(paquete_a_enviar, texto, size);
                    enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
                    free(texto);
                }
                free(resultado_lectura);
                break;
            case PEDIDO_ESCRITURA:
                int resultado_escritura=hacer_pedido_escritura(lista);
                if (resultado_escritura == 0)
                {
                    enviar_mensaje("OK", cliente_cpu, logger);
                }
                break;
            case OBTENER_MARCO:
                uint32_t marco= obtener_marco(lista);
                t_paquete *paquete_a_enviar = crear_paquete();
                agregar_a_paquete(paquete_a_enviar, marco, sizeof(uint32_t));
                enviar_paquete(paquete_a_enviar, cliente_cpu, logger);
                break;
            case OBTENER_INSTRUCCION:
                log_debug(logger, "Voy a obtener la instruccion");
                char* linea_de_instruccion= obtener_instruccion(lista);
                log_debug(logger, "La linea de codigo: %s", linea_de_instruccion);
                enviar_mensaje(linea_de_instruccion, cliente_cpu, logger);
                free(linea_de_instruccion);
                break;
            case RESIZE_PROCESO:
                int resultado=modificar_tamanio_proceso(lista); // 1 se pudo, 0 no se pudo
                t_paquete *paquete = crear_paquete();
                agregar_a_paquete(paquete, resultado, sizeof(int));
                enviar_paquete(paquete, cliente_cpu, logger);
                break;
            default: // recibi algo q no es eso, vamos a suponer q es para terminar
                log_error(logger, "Desde cliente: %d Recibi una operacion de memoria rara (%d), termino el servidor.", operacion_memoria);
                return EXIT_FAILURE;
            }
            break;
        case EXIT: // indica desconexion
            log_error(logger, "Se desconecto el cliente %d.", cliente_cpu);
            sigo_funcionando = 0;
            return EXIT_FAILURE;
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

                    int resultado_escritura=hacer_pedido_escritura(lista);
                    if (resultado_escritura == 0)
                    {
                        enviar_mensaje("OK", cliente_entradasalida, logger);
                    }
                    break;
                case PEDIDO_LECTURA:
                    uint8_t tipo = (uint8_t)list_get(lista, 4);
                    uint32_t size = (uint32_t)list_get(lista, 2);
                    log_debug(logger,"prueba");
                    void* resultado_lectura=hacer_pedido_lectura(lista);
                    if (tipo){
                        uint32_t valor = *(uint32_t*)resultado_lectura;
                        t_paquete *paquete_a_enviar = crear_paquete();
                        agregar_a_paquete(paquete_a_enviar, valor, size);
                        enviar_paquete(paquete_a_enviar, cliente_entradasalida, logger);
                    }
                    else{
                        char* texto = string_duplicate((char*)resultado_lectura);
                        t_paquete *paquete_a_enviar = crear_paquete();
                        agregar_a_paquete(paquete_a_enviar, texto, size);
                        enviar_paquete(paquete_a_enviar, cliente_entradasalida, logger);
                        free(texto);
                    }
                    free(resultado_lectura);
                    break;
            }
            break;
        case EXIT: // indica desconexion
            log_error(logger, "Se desconecto el cliente %d.", cliente_entradasalida, cliente_entradasalida);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Desde cliente %d: Recibi una operacion rara (%d), termino el servidor.", cliente_entradasalida, operacion);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

void* hacer_pedido_lectura(t_list* lista){ 
    uint32_t df = (uint32_t)list_get(lista, 1);
    uint32_t size = (uint32_t)list_get(lista, 2);
    uint32_t pid = (uint32_t)list_get(lista, 3);
    uint8_t tipo = (uint8_t)list_get(lista, 4);
    log_debug(logger,"PID: <%u> - size: <%u> - df: <%u> - tipo: <%u>", pid, size, df, tipo);
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
        log_debug(logger,"Valor leído: %u", valor);
        resultado = malloc(sizeof(uint32_t));
        *(uint32_t*)resultado = valor; //convierto resultado a un puntero a uint32_t y guardo el valor a donde apunta
    }
    else {
        resultado=(void*)string_substring_until(elemento_a_guardar,size_original);
        log_info(logger,"Lo que lei fue este texto: %s",resultado);
    }
    free(elemento_a_guardar);
    return resultado;
}

int hacer_pedido_escritura(t_list* lista){ // escribe en memoria en Little Endian (de derecha a izquierda)
        uint32_t df = (uint32_t)list_get(lista, 1);
        uint32_t size = (uint32_t)list_get(lista, 2);
        uint32_t pid = (uint32_t)list_get(lista, 3);
        void *elemento_a_insertar  = list_get(lista, 4);
        uint8_t tipo = (uint8_t)list_get(lista, 5); // 0 si es char* y 1 si es uint32_t
        int resultado = -1; 
        uint32_t marco = floor(df / tam_pag);
        log_debug(logger, "Marco donde arranca la escritura: %d", marco);
        log_info(logger,"PID: <%u> - Accion: <ESCRIBIR> - Direccion fisica: %u - Tamaño <%u>", pid, df, size);
        
        while (size) {
            uint32_t memoria_disponible  = ((marco + 1) * tam_pag - df);
            log_debug(logger, "Bytes disponible en este marco: %d", memoria_disponible);
            log_debug(logger, "Tamaño en bytes que tengo que escribir: %d", size);
            if (memoria_disponible >= size) {
                if (tipo)
                {
                    memcpy(espacio_memoria + df, &elemento_a_insertar, size);
                    resultado = 0;
                }
                else
                {
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
                log_debug(logger, "Avanzo al marco: %u", marco);
                df = marco * tam_pag;
                log_debug(logger, "Direccion fisica donde comienzo a escribir: %u", df);
            }
        }
        mem_hexdump(espacio_memoria, tam_memoria);
        return resultado;
}

char* obtener_instruccion(t_list* lista){
    uint32_t pid = (uint32_t)list_get(lista, 1);
    uint32_t pc = (uint32_t)list_get(lista, 2);
    log_debug(logger, "El pid es: %d y el pc es: %d", pid, pc);
    t_memoria_proceso *proceso_actual = encontrar_proceso(pid);
    char *linea_de_instruccion = string_duplicate(proceso_actual->lineas_de_codigo[pc]);
    return linea_de_instruccion;
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

    log_info(logger, "PID: <%u> - Tamaño: <0>", process_id);
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
    log_info(logger, "PID: <%u> - Tamaño: <%u>", pid, proceso->paginas_actuales);
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
    tam_memoria = config_get_int_value(config, "TAM_MEMORIA"); // 32 bytes
    cant_marcos = tam_memoria / tam_pag; // tam_pag = 4 bytes => 8 marcos
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
    // hay Segmentacion Fault
    int i = paginas_actuales-1;
    while (i >= (int)paginas_necesarias) // lo convierto en int para que la comparacion permite al -1 en i
    {
        // modifico el bitmap marcos para que el marco vuelva a estar libre
        bitarray_clean_bit(marcos, proceso->tabla_paginas[i]);
        proceso->tabla_paginas[i]=0;
        proceso->paginas_actuales--;
        i--;
    }
}

int agregar_paginas(t_memoria_proceso* proceso,uint32_t paginas_actuales,uint32_t paginas_necesarias)
{
    for (int i = paginas_actuales; i < paginas_necesarias; i++)
    {
        // busco un marco libre
        int marco_libre = bitarray_buscar_libre();
        if (marco_libre == -1)
        {
            log_error(logger, "Out Of Memory");
            return 1;
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

int bitarray_buscar_libre()
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

uint32_t obtener_marco(t_list* lista)
{
    uint32_t pid = (uint32_t)list_get(lista, 1);
    uint32_t pagina = (uint32_t)list_get(lista, 2);
    t_memoria_proceso *proceso = encontrar_proceso(pid);
    uint32_t marco = proceso->tabla_paginas[pagina];
    log_info(logger, "PID: <%u> - Pagina: <%u> - Marco: <%u>\n", pid, pagina, marco);
    return marco;
}

int modificar_tamanio_proceso(t_list* lista) 
{
    //la lista es [MODIFICAR_TAMANIO_PROCESO, nuevo_tam, pid]
    int resultado = 0; // 1 si no se pudo agregar paginas, 0 si se pudo

    // conseguir el nuevo tamaño del proceso
    uint32_t nuevo_tam = (uint32_t)list_get(lista, 1); //es en bytes

    // nuevo tam divido tam de pag para saber cuantas paginas necesito
    // si no las tengas voy desde la primera que necesito en adelante
    uint32_t pid = (uint32_t)list_get(lista, 2);
    t_memoria_proceso *proceso = encontrar_proceso(pid);
    uint32_t paginas_necesarias = ceil(nuevo_tam / (uint32_t)tam_pag); // pense que ya redondeaba para arriba, fijarme eso!!
    uint32_t paginas_actuales = proceso->paginas_actuales;
    if (paginas_actuales>paginas_necesarias)
    {
        log_info(logger, "PID: <%u> - Tamaño Actual: <%u> - Tamaño a Reducir: <%u>", pid, paginas_actuales, paginas_necesarias);
        borrar_paginas(proceso, paginas_necesarias, paginas_actuales);
    }
    else if (paginas_actuales<paginas_necesarias)
    {
        log_info(logger, "PID: <%u> - Tamaño Actual: <%u> - Tamaño a Ampliar: <%u>", pid, paginas_actuales, paginas_necesarias);
        resultado = agregar_paginas(proceso, paginas_actuales, paginas_necesarias);
    }
    return resultado;
}

void inicializar_modulo_memoria(int argc, char *argv[])
{
    procesos = dictionary_create();
    logger = iniciar_logger("memoria.log", "MEMORIA", argc, argv);
    config = iniciar_config("memoria.config");
    tam_pag = config_get_int_value(config, "TAM_PAGINA");
    path = config_get_string_value(config, "PATH_INSTRUCCIONES");
    crear_espacio_memoria();

}

void testear_modulo_memoria()
{
    // por ahora acordarse que la memoria es de 64 bytes y las paginas de 4 bytes -> total de paginas 16
    crear_proceso("prueba", 1);
    crear_proceso("prueba2", 2);
    crear_proceso("pk", 3);
    // para asegurarnos que anda bien modificar_tamanio_proceso 
    //vamos a usarlo para asignas bytes a los procesos
    // la lista de modificar_tamanio_proceso es [MODIFICAR_TAMANIO_PROCESO, nuevo_tam, pid]
    t_list* listaza = list_create();
    list_add(listaza, MODIFICAR_TAMANIO_PROCESO); list_add(listaza, 16); list_add(listaza, 1);
    modificar_tamanio_proceso(listaza);
    list_clean(listaza); list_add(listaza, MODIFICAR_TAMANIO_PROCESO); list_add(listaza, 8); list_add(listaza, 2);
    modificar_tamanio_proceso(listaza);
    list_clean(listaza); list_add(listaza, MODIFICAR_TAMANIO_PROCESO); list_add(listaza, 32); list_add(listaza, 3);
    modificar_tamanio_proceso(listaza);
    list_clean(listaza); list_add(listaza, MODIFICAR_TAMANIO_PROCESO); list_add(listaza, 20); list_add(listaza, 1);
    modificar_tamanio_proceso(listaza); 
    // la memoria esta llena, probar el error
    list_clean(listaza); list_add(listaza, MODIFICAR_TAMANIO_PROCESO); list_add(listaza, 16); list_add(listaza, 2);
    int resultado=modificar_tamanio_proceso(listaza);
    assert(resultado == 1);
    // ahora a empezar a escribir y leer
    // la lista de pedido_escritura es [PEDIDO_ESCRITURA, df, size, pid, elemento_a_insertar, tipo]
    // ir haciendo tanto de char* como de uint32_t o de uint8_t
    // todo es dentro de una lista :(
    

    //haceme todo lo que hice arriba pero que a hacer_pedido_lectura o pedido_escritura le pases una lista
    t_list* lista = list_create();
    list_add(lista, PEDIDO_ESCRITURA); list_add(lista, 0); list_add(lista, 4); list_add(lista, 1); list_add(lista, "hola"); list_add(lista, 0);
    hacer_pedido_escritura(lista);
    list_clean(lista); list_add(lista, PEDIDO_ESCRITURA); list_add(lista, 4); list_add(lista, 4); list_add(lista, 1); list_add(lista, "chau"); list_add(lista, 0);
    hacer_pedido_escritura(lista);
    list_clean(lista); list_add(lista, PEDIDO_ESCRITURA); list_add(lista, 8); list_add(lista, 2); list_add(lista, 1); list_add(lista, 128); list_add(lista, 1);
    // 128 es en hexa 0x80
    hacer_pedido_escritura(lista);
    list_clean(lista); list_add(lista, PEDIDO_ESCRITURA); list_add(lista, 10); list_add(lista, 2); list_add(lista, 1); list_add(lista, 255); list_add(lista, 1);
    // 254 es en hexa 0xFF
    hacer_pedido_escritura(lista);
    list_clean(lista); list_add(lista, PEDIDO_ESCRITURA); list_add(lista, 56); list_add(lista, 4); list_add(lista, 2); list_add(lista, "cera"); list_add(lista, 0);
    hacer_pedido_escritura(lista);
    list_clean(lista); list_add(lista, PEDIDO_ESCRITURA); list_add(lista, 60); list_add(lista, 4); list_add(lista, 2); list_add(lista, 12800); list_add(lista, 1);
    // 12800 es en hexa 0x3200
    hacer_pedido_escritura(lista);
    list_clean(lista); list_add(lista, PEDIDO_ESCRITURA); list_add(lista, 26); list_add(lista, 2); list_add(lista, 3); list_add(lista, 0); list_add(lista, 1);
    // 0 es en hexa 0x00
    hacer_pedido_escritura(lista);
    list_clean(lista); list_add(lista, PEDIDO_ESCRITURA); list_add(lista, 28); list_add(lista, 2); list_add(lista, 3); list_add(lista, 244); list_add(lista, 1);
    // 244 es en hexa 0xF4
    hacer_pedido_escritura(lista);

    list_clean(lista); list_add(lista, PEDIDO_LECTURA); list_add(lista, 0); list_add(lista, 4); list_add(lista, 1); list_add(lista, 0);
    char* resultado_palabra = (char*) hacer_pedido_lectura(lista);
    assert(strcmp(resultado_palabra, "hola") == 0);
    list_clean(lista); list_add(lista, PEDIDO_LECTURA); list_add(lista, 4); list_add(lista, 4); list_add(lista, 1); list_add(lista, 0);
    resultado_palabra = (char*) hacer_pedido_lectura(lista);
    assert(strcmp(resultado_palabra, "chau") == 0);
    list_clean(lista); list_add(lista, PEDIDO_LECTURA); list_add(lista, 8); list_add(lista, 2); list_add(lista, 1); list_add(lista, 1);
    uint32_t* resultado_int = (uint32_t*) hacer_pedido_lectura(lista);
    assert(*resultado_int == 128);
    list_clean(lista); list_add(lista, PEDIDO_LECTURA); list_add(lista, 10); list_add(lista, 2); list_add(lista, 1); list_add(lista, 1);
    resultado_int = (uint32_t*) hacer_pedido_lectura(lista);
    assert(*resultado_int == 255);
    list_clean(lista); list_add(lista, PEDIDO_LECTURA); list_add(lista, 56); list_add(lista, 4); list_add(lista, 2); list_add(lista, 0);
    resultado_palabra = (char*) hacer_pedido_lectura(lista);
    assert(strcmp(resultado_palabra, "cera") == 0);
    list_clean(lista); list_add(lista, PEDIDO_LECTURA); list_add(lista, 60); list_add(lista, 4); list_add(lista, 2); list_add(lista, 1);
    resultado_int = (uint32_t*) hacer_pedido_lectura(lista);
    assert(*resultado_int == 12800);
    list_clean(lista); list_add(lista, PEDIDO_LECTURA); list_add(lista, 26); list_add(lista, 2); list_add(lista, 3); list_add(lista, 1);
    resultado_int = (uint32_t*) hacer_pedido_lectura(lista);
    assert(*resultado_int == 0);
    list_clean(lista); list_add(lista, PEDIDO_LECTURA); list_add(lista, 28); list_add(lista, 2); list_add(lista, 3); list_add(lista, 1);
    resultado_int = (uint32_t*) hacer_pedido_lectura(lista);
    assert(*resultado_int == 244);
    

    destruir_proceso(1);
    destruir_proceso(2);
    destruir_proceso(3);
    // tendria que hacer un assert de que el bitmat esta vacio
    for (int i = 0; i < cant_marcos; i++)
    {
        assert(bitarray_test_bit(marcos, i) == 0);
    }
    // si llegaste hasta aca, felicidades, pasaste el testeo
    log_debug(logger, "Testeo de memoria exitoso");

}


void finalizar_modulo_memoria()
{
    pthread_join(tid[SOY_CPU], NULL);
    pthread_join(tid[SOY_KERNEL], NULL);
    dictionary_destroy(procesos);
    free(espacio_memoria);
    bitarray_destroy(marcos);
    destruir_logger(logger);
    destruir_config(config);
}

void creacion_servidor_y_clientes()
{
    socket_servidor = iniciar_servidor(config, "PUERTO_ESCUCHA");
    log_info(logger, "Servidor %d creado.", socket_servidor);

    // 3 clientes - CPU - I/O - KERNEL
    // Sabemos que los 3 clientes se conectan en orden: CPU => KERNEL => I/O

    // Recibimos la conexion de la CPU y creamos un hilo para trabajarlo
    cliente_cpu = esperar_cliente(socket_servidor, logger);
    pthread_create(&tid[SOY_CPU], NULL, servidor_cpu, NULL);

    // Recibimos la conexion de la kernel y creamos un hilo para trabajarlo
    cliente_kernel = esperar_cliente(socket_servidor, logger);
    pthread_create(&tid[SOY_KERNEL], NULL, servidor_kernel, NULL);

    // Recibimos las multiples conexiones de la I/O y creamos los hilo para trabajar
    pthread_create(&tid[SOY_IO], NULL, esperar_io, NULL);

}