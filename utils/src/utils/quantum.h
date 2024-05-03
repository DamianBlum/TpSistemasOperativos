#ifndef PCB_H_
#define PCB_H_

#include <netdb.h>
#include <time.h>
#include <commons/log.h>

// esto hace el sleep
void iniciar_quantum(uint32_t tiempo_en_ms);
void iniciar_cronometro(t_log *logger);
uint32_t terminar_cronometro();

#endif