#include "main.h"

t_log *logger;
t_config *config;

// servidor
int socket_servidor;

// clientes
int cliente_kernel;
int cliente_cpu;
int cliente_entradasalida;

// hilos
pthread_t tid[3];

FILE * archivo_text_proceso;
// diccionario de procesos
t_dictionary *procesos;

// path de los archivos
char *path;

int main(int argc, char *argv[])
{

    procesos = dictionary_create();
    logger = iniciar_logger("memoria.log", "MEMORIA", argc, argv);
    config = iniciar_config("memoria.config");
    path = config_get_string_value(config, "PATH_INSTRUCCIONES");

    //Para mostrar Funcionamiento de crear_proceso
    crear_proceso("prueba.txt", 1);
    crear_proceso("prueba2.txt", 2);
    // consigo el proceso 1
    t_memoria_proceso *proceso = dictionary_get(procesos, "1");
    printf("El proceso 1 tiene el archivo %s\n", proceso->nombre_archivo);
   

    
    /*
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

    // Recibimos la conexion de la I/O y creamos un hilo para trabajarlo
    cliente_entradasalida = esperar_cliente(socket_servidor, logger);
    pthread_create(&tid[SOY_IO], NULL, servidor_entradasalida, NULL);

    pthread_join(tid[SOY_CPU], NULL);
    pthread_join(tid[SOY_KERNEL], NULL);
    pthread_join(tid[SOY_IO], NULL);*/
    
    destruir_logger(logger);
    destruir_config(config);

    return EXIT_SUCCESS;
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
                log_info(logger, "Desde cliente %d: Recibi el mensaje: %s.", cliente_cpu, mensaje);
                // hago algo con el mensaje
                break;
            case PAQUETE:
                t_list *lista = list_create();
                lista = recibir_paquete(cliente_kernel, logger);
                log_info(logger, "Desde cliente %d: Recibi un paquete.", cliente_cpu);
                e_operacion operacion_kernel= (e_operacion)list_get(lista, 0);
                switch (operacion_kernel)
                {

                    case INICIAR_PROCESO:
                        char* nombre_archivo = list_get(lista, 1);
                        uint32_t process_id = (uint32_t)list_get(lista, 2);
                        int resultado=crear_proceso(nombre_archivo, process_id);

                        if(resultado==0){
                            log_info(logger, "Se creo el proceso %d con el archivo %s.", process_id, nombre_archivo);
                        }
                        else{
                            log_error(logger, "No se pudo crear el proceso %d con el archivo %s.", process_id, nombre_archivo);
                        }

                        t_paquete* paquete_a_enviar=crear_paquete();
                        agregar_a_paquete(paquete_a_enviar, resultado, sizeof(bool));
                        enviar_paquete(paquete_a_enviar,cliente_kernel, logger);
                        eliminar_paquete(paquete_a_enviar);

                        break;
                    case BORRAR_PROCESO:
                        uint32_t pid = (uint32_t)list_get(lista, 1);
                        destruir_proceso(pid);
                        break;
                    default:
                        break;
                }
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
    archivo_text_proceso = fopen("prueba.txt", "r");
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
            
            uint32_t pid = (uint32_t)list_get(lista, 0);
            uint32_t pc = (uint32_t)list_get(lista, 1);
            t_memoria_proceso* proceso_actual=encontrar_proceso(pid);


            enviar_mensaje(cliente_cpu, "owo", logger);
            break;
        case EXIT: // indica desconeccion
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
            // hago algo con el mensaje
            break;
        case EXIT: // indica desconeccion
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


int crear_proceso(char* nombre_archivo, uint32_t process_id){
    char **lineas = string_array_new();
    char *linea, longitud[100];

    log_debug(logger, "Voy a crear el proceso %d con el archivo %s", process_id, nombre_archivo);

    char * path_completo = string_from_format("%s%s", path,nombre_archivo);

    log_debug(logger, "El path del archivo es: %s", path_completo);

    // abro el archivo que queda en la carpeta de la memoria que saco del config en PATH_INSTRUCCIONES con nombre de archivo nombre_archivo
    archivo_text_proceso = fopen(path_completo, "r");
    
    // leo cada linea y la imprimo por pantalla
    if (archivo_text_proceso == NULL) {
        log_error(logger,"Error al abrir el archivo");
        return 1;
    }

    while (fgets(longitud, 100, archivo_text_proceso) != NULL)
    {
        linea = string_duplicate(longitud);
        string_array_push(&lineas, linea);
    }
    
    fclose(archivo_text_proceso);

    t_memoria_proceso* proceso=crear_estructura_proceso(nombre_archivo, lineas);

    //lo guardo en mi diccionario de procesos
    // paso de uint32_t a string
    char* string_pid = string_itoa((int)process_id);

    dictionary_put(procesos, string_pid, proceso);

    log_debug(logger," Syze del diccionario: %d\n", dictionary_size(procesos));

    // libero variables
    free(path_completo);
    free(linea);
    free(string_pid);

    printf("prueba");
    return 0;
}

t_memoria_proceso* crear_estructura_proceso(char* nombre_archivo, char** lineas_de_codigo){
    // Creo un proceso con el nombre del archivo y el vector posiciones
    t_memoria_proceso *proceso = malloc(sizeof(t_memoria_proceso));
    proceso->nombre_archivo = string_duplicate(nombre_archivo);
    proceso->lineas_de_codigo = lineas_de_codigo;
    return proceso;
}

t_memoria_proceso* encontrar_proceso(uint32_t pid){
    // busco el proceso en el diccionario de procesos
    char* string_pid = string_itoa((int)pid);
    t_memoria_proceso *proceso = dictionary_get(procesos, string_pid);
    free(string_pid);
    return proceso;
}

void destruir_proceso(uint32_t pid){
    // busco el proceso en el diccionario de procesos
    char* string_pid = string_itoa((int)pid);
    t_memoria_proceso *proceso = dictionary_remove(procesos, string_pid);
    // libero la memoria del proceso
    free(proceso->nombre_archivo);
    string_array_destroy(proceso->lineas_de_codigo);
    free(proceso);
    free(string_pid);

}

char* devolver_linea(t_memoria_proceso* proceso, uint32_t pc, FILE* codigo_proceso_actual){

}