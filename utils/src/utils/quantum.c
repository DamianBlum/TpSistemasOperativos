#include "quantum.h"

clock_t momento_inicio, diferencia;

void iniciar_quantum(uint32_t tiempo_en_ms)
{
    uint32_t q_en_s = tiempo_en_ms / 1000; // como sleep es en segundos, hago la cuentita para pasarlo de ms a s (terrible cuenta), supongo q no importa, pero x el tipo de dato q lo guardo, se queda en lo entero, osea 2500 ms => 2s
    sleep(q_en_s);
}

void iniciar_cronometro(t_log *logger)
{
    log_trace(logger, "Arranco el cronometro");
    momento_inicio = clock();
    log_debug(logger, "Instante: %d", momento_inicio);
}

uint32_t terminar_cronometro() // devuelve en ms el tiempo
{
    diferencia = clock() - momento_inicio;
    printf("Dif: %d\n", diferencia);
    // medio rara esta cuenta, pero lo paso a segundos y desp a ms
    printf("ASD: %d\n", diferencia / CLOCKS_PER_SEC);
    return (diferencia * 1000 / CLOCKS_PER_SEC);
}