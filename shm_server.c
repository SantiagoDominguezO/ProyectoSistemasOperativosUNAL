#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "shm_comun.h"

static void al_terminar(int sig)
{
    (void)sig;
    shm_unlink(SHM_NOMBRE);
    _exit(0);
}

int main(int argc, char *argv[])
{
    const char *csv = (argc > 1) ? argv[1] : CSV_PATH;

    FILE *prueba = fopen(csv, "r");
    if (prueba == NULL) {
        perror("No se pudo abrir el CSV");
        return 1;
    }
    fclose(prueba);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = al_terminar;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    shm_unlink(SHM_NOMBRE);   /* por si quedo una de una ejecucion anterior */
    int fd = shm_open(SHM_NOMBRE, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd == -1) { perror("shm_open"); return 1; }

    if (ftruncate(fd, sizeof(Compartida)) == -1) {
        perror("ftruncate");
        close(fd);
        shm_unlink(SHM_NOMBRE);
        return 1;
    }

    Compartida *c = mmap(NULL, sizeof(Compartida), PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0);
    if (c == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(SHM_NOMBRE);
        return 1;
    }
    close(fd);   /* el mapeo sigue valido sin el descriptor */

    memset(c, 0, sizeof(Compartida));
    /* pshared = 1: semaforo compartido entre procesos; valor inicial 0 */
    if (sem_init(&c->sem_solicitud, 1, 0) == -1 ||
        sem_init(&c->sem_respuesta, 1, 0) == -1) {
        perror("sem_init");
        munmap(c, sizeof(Compartida));
        shm_unlink(SHM_NOMBRE);
        return 1;
    }

    printf("Servidor SHM (PID %d) usando %s. Esperando solicitudes...\n", getpid(), csv);

    while (1) {
        /* Bloquea (sin gastar CPU) hasta que el cliente avise */
        if (sem_wait(&c->sem_solicitud) == -1) {
            if (errno == EINTR) continue;
            perror("sem_wait");
            break;
        }

        if (c->tipo == TIPO_SALIR) {
            c->n = 0;
            c->estado = 0;
            sem_post(&c->sem_respuesta);
            break;
        }

        c->valor[MAX_VALOR - 1] = '\0';
        c->n = buscar(csv, c->criterio, c->valor, c->cuerpo, sizeof(c->cuerpo));
        c->estado = (c->n < 0) ? -1 : 0;
        sem_post(&c->sem_respuesta);      /* avisar al cliente que ya puede leer */
    }

    sem_destroy(&c->sem_solicitud);
    sem_destroy(&c->sem_respuesta);
    munmap(c, sizeof(Compartida));
    shm_unlink(SHM_NOMBRE);
    printf("Servidor terminado.\n");
    return 0;
}

