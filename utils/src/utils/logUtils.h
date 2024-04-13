#ifndef LOGGER_UTILS_H_
#define LOGGER_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/string.h>

t_log *iniciar_logger(char *nombreLog, char *rutaLog, int argc, char *argv[]);
void destruir_logger(t_log *logger);

#endif /* LOGGER_UTILS_H_ */