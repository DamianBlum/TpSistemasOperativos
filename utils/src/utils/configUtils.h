#ifndef UTILS_CONFIG_H_
#define UTILS_CONFIG_H_

#include <stdlib.h>
#include <stdio.h>
#include <commons/config.h>

t_config *iniciar_config(char *nombreArchivo);
void destruir_config(t_config *config);

#endif
