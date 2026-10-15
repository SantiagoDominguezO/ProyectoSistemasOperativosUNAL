#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "shm_comun.h"

#define ESPERA_MAX 10   /* segundos que se espera la respuesta del servidor */

/* Espera la respuesta con limite de tiempo. Retorna -1 si no llega. */
static int esperar_respuesta(Compartida *c)
{
    struct timespec limite;
    clock_gettime(CLOCK_REALTIME, &limite);
    limite.tv_sec += ESPERA_MAX;

    while (sem_timedwait(&c->sem_respuesta, &limite) == -1) {
        if (errno == EINTR) continue;
        if (errno == ETIMEDOUT)
            printf("El servidor no respondio en %d segundos.\n", ESPERA_MAX);
        else
            perror("sem_timedwait");
        return -1;
    }
    return 0;
}

int main(void)
{
    int fd = shm_open(SHM_NOMBRE, O_RDWR, 0);
    if (fd == -1) {
        if (errno == ENOENT)
            printf("El servidor no esta en ejecucion.\n");
        else
            perror("shm_open");
        return 1;
    }

    Compartida *c = mmap(NULL, sizeof(Compartida), PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0);
    if (c == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    close(fd);

    while (1) {
        printf("\n1) Buscar por ID\n2) Buscar por ciudad\n3) Salir\nOpcion: ");
        char entrada[64];
        if (fgets(entrada, sizeof(entrada), stdin) == NULL)
            break;
        int op = atoi(entrada);

        if (op == 3) {
            c->tipo = TIPO_SALIR;
            sem_post(&c->sem_solicitud);
            esperar_respuesta(c);
            printf("Cliente y servidor finalizados.\n");
            break;
        }
        if (op != 1 && op != 2) {
            printf("Opcion invalida.\n");
            continue;
        }

        printf(op == 1 ? "ID: " : "Ciudad: ");
        char valor[MAX_VALOR];
        if (fgets(valor, sizeof(valor), stdin) == NULL)
            break;
        valor[strcspn(valor, "\r\n")] = '\0';
        if (valor[0] == '\0') {
            printf("Valor invalido.\n");
            continue;
        }

        /* 1. escribir la solicitud (el servidor esta dormido en sem_wait) */
        c->tipo = TIPO_CONSULTA;
        c->criterio = op;
        strncpy(c->valor, valor, MAX_VALOR - 1);
        c->valor[MAX_VALOR - 1] = '\0';

        /* 2. avisar al servidor */
        sem_post(&c->sem_solicitud);

        /* 3. dormir hasta que el servidor termine de escribir la respuesta */
        if (esperar_respuesta(c) < 0)
            break;

        /* 4. ahora si es seguro leer */
        if (c->estado < 0)
            printf("Error: el servidor no pudo leer el CSV.\n");
        else if (c->n == 0)
            printf("No se encontraron resultados.\n");
        else
            printf("Resultados (%d):\n%s", c->n, c->cuerpo);
    }

    munmap(c, sizeof(Compartida));
    return 0;
}

