#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/logUtils.h>
#include <utils/configUtils.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <utils/sockets.h>
#include <commons/bitarray.h>
#include <commons/memory.h>
#include <../src/main.c>
#include <../src/main.h>
#include "cspecs/cspec.h"

context (pruebas) {
    describe("Pruebas de la memoria") {
        it("Deberia devolver el marco 0") {
            uint32_t pid = 1;
            uint32_t pagina = 0;
            uint32_t marco = devolver_marco(pid, pagina);
            should_int(marco) be equal to(0);
        } end
    } end
}