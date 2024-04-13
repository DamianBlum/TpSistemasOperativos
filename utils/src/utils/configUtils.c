#include "configUtils.h"

t_config *iniciar_config(char *nombreArchivo)
{
    t_config *nuevo_config;
    nuevo_config = config_create(nombreArchivo);
    return nuevo_config;
}

void destruir_config(t_config *config)
{
    config_destroy(config);
}