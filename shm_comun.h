#ifndef SHM_COMUN_H
#define SHM_COMUN_H

#include <semaphore.h>
#include "busqueda.h"

#define SHM_NOMBRE     "/so_practica1_shm"   /* aparece en /dev/shm */
#define TIPO_CONSULTA  1
#define TIPO_SALIR     2

typedef struct {
    sem_t sem_solicitud;     /* el cliente la "postea" cuando la solicitud esta lista  */
    sem_t sem_respuesta;     /* el servidor la "postea" cuando la respuesta esta lista */
    int   tipo;              /* TIPO_CONSULTA o TIPO_SALIR */
    int   criterio;
    char  valor[MAX_VALOR];
    int   estado;            /* 0 = ok, -1 = error en el servidor */
    int   n;                 /* numero de coincidencias */
    char  cuerpo[MAX_CUERPO];
} Compartida;

#endif

