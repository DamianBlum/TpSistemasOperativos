#include "main.h"
#include "cicloDeInstruccion.h"

// Servidor
int socket_servidor_dispatch;
int socket_servidor_interrupt;
int socket_cliente_dispatch;
int socket_cliente_interrupt;

// Clientes
int cliente_memoria;

t_log *logger;
t_config *config;

// hilos
pthread_t tid[2];

// registros 
t_registros *registros;

// bit para la interrupion
bool interrupcion;

// Linea de instruccion que llega de memoria
char* linea_de_instruccion;

// instruccion
e_instruccion instruccion;

// Linea de instruccion separada
char ** linea_de_instruccion_separada;



int main(int argc, char *argv[])
{
    registros = crear_registros();
    logger = iniciar_logger("cpu.log", "CPU", argc, argv);
    config = iniciar_config("cpu.config");

    // Parte cliente, por ahora esta harcodeado, despues agregar
    /* 
    cliente_memoria = crear_conexion(config, "IP_MEMORIA", "PUERTO_MEMORIA", logger);
    enviar_mensaje("Hola Memoria soy yo, tu cliente CPU :D", cliente_memoria, logger);
    */
    // PARTE SERVIDOR

    pthread_create(&tid[DISPATCH], NULL, servidor_dispatch, NULL);

    pthread_create(&tid[INTERRUPT], NULL, servidor_interrupt, NULL);

    // acto de ejectar una instruccion

    fetch(instruccion, registros->PC, logger);
    decode(linea_de_instruccion,instruccion, linea_de_instruccion_separada, logger);
    execute(linea_de_instruccion_separada,instruccion,registros,logger);

    /*
    while (1) 
    {
        //como sabemos si los registros tienen algo al principio?
        //como sabemos cuanto esperar despues de ejecutar una instruccion para seguir esto?



        fetch( instruccion, registros->PC, logger); 
        //busca con registros->PC en la memoria 
        //y devuelve un string por ahora me imagine con una instruccion de esta forma "SUM AX BX" 

        decode(linea_de_instruccion, instruccion, linea_de_instruccion_separada, logger); 
        //consigue la primera palabra que seria el "SUM" y en base a eso en un switch o algo de ese estilo despues ve que seria "AX" y "BX"

        execute(linea_de_instruccion_separada,instruccion,registros,logger); 
        //Ejecuta la instruccion y guarda la data en los registros que se actualizaro y aumenta el PC en uno si no fue un EXIT
        // a la vez si es exucute de un exit deberia finalizar la ejecucion de este proceso no se como
        // write back es como usar el log, ahi se actualiza el PC en uno
        
        check_interrupt();
        // se fija si esta la interrupcion de eliminar_proceso
        // y el cpu carga despues nuevo_registros

        //recordar que las unicas interrupciones son o por quantum o por entrada salida que pida la instruccion!
        // la unica interrupcion exterena es de cuando se quiere eliminar un proceso
        registros->PC++;
    }
    */
    pthread_join(tid[DISPATCH], NULL);
    pthread_join(tid[INTERRUPT], NULL);



    // liberar_conexion(cliente_memoria, logger); cuando pruebe lo de ser cliente de memoria descomentar esto

    // libero todas las variables que uso
    free(linea_de_instruccion);
    destruir_registros(registros);
    destruir_config(config);
    destruir_logger(logger);
    string_array_destroy(linea_de_instruccion_separada);

    return 0;
}

void *servidor_dispatch(void *arg)
{
    socket_servidor_dispatch = iniciar_servidor(config, "PUERTO_ESCUCHA_DISPATCH");

    log_info(logger, "Servidor Dispatch %d creado.", socket_servidor_dispatch);

    socket_cliente_dispatch = esperar_cliente(socket_servidor_dispatch, logger); 

    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(socket_cliente_dispatch, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(socket_cliente_dispatch, logger);
            log_info(logger, "Recibi el mensaje: %s.", mensaje);
            // se supone que no se usa mucho por el dispatch un mensaje tipo string la verdad
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(socket_cliente_dispatch, logger);
            log_info(logger, "Recibi un paquete.");
            // deberia recibir por aca un PCB para ponerme a ejercutar las instrucciones en el hilo principal  
            desempaquetar_pcb_a_registros(lista,registros,logger); //aca pasa de los paquete a los registros, por ahora ignora el quantum, despues hablarlo con pepo si lo puedo hacer 

            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", socket_cliente_dispatch);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Recibi una operacion rara (%d), termino el servidor.", operacion);
            return EXIT_FAILURE;
        }
    }
}

void *servidor_interrupt(void *arg) // por aca va a recibir un bit cuando quiere eliminar un proceso de una
{
    socket_servidor_interrupt = iniciar_servidor(config, "PUERTO_ESCUCHA_INTERRUPT");

    log_info(logger, "Servidor Interrupt %d creado.", socket_servidor_interrupt);

    socket_cliente_interrupt = esperar_cliente(socket_servidor_interrupt, logger);

    int sigo_funcionando = 1;
    while (sigo_funcionando)
    {
        int operacion = recibir_operacion(socket_cliente_interrupt, logger);

        switch (operacion) // MENSAJE y PAQUETE son del enum op_code de sockets.h
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(socket_cliente_interrupt, logger);
            log_info(logger, "Recibi el mensaje: %s.", mensaje);
            // hago algo con el mensaje
            break;
        case PAQUETE:
            t_list *lista = list_create();
            lista = recibir_paquete(socket_cliente_interrupt, logger);
            log_info(logger, "Recibi un paquete.");
            // hago algo con el paquete
            break;
        case EXIT: // indica desconeccion
            log_error(logger, "Se desconecto el cliente %d.", socket_cliente_interrupt);
            sigo_funcionando = 0;
            break;
        default: // recibi algo q no es eso, vamos a suponer q es para terminar
            log_error(logger, "Recibi una operacion rara (%d), termino el servidor.", operacion);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
