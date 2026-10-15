#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "busqueda.h"
#include "fifo_comun.h"

/* Si el servidor recibe Ctrl+C, borra las FIFO antes de morir. */
static void al_terminar(int sig)
{
    (void)sig;
    unlink(FIFO_REQ);
    unlink(FIFO_RESP);
    _exit(0);
}

static int crear_fifo(const char *ruta)
{
    if (mkfifo(ruta, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        return -1;
    }
    return 0;
}

/* Envia: "TIPO|n|len\n" seguido de len bytes de cuerpo. */
static int responder(int fd, const char *tipo, int n, const char *cuerpo)
{
    char cab[64];
    int len = (int)strlen(cuerpo);
    snprintf(cab, sizeof(cab), "%s|%d|%d\n", tipo, n, len);
    if (escribir_todo(fd, cab, strlen(cab)) < 0) return -1;
    if (len > 0 && escribir_todo(fd, cuerpo, (size_t)len) < 0) return -1;
    return 0;
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
    signal(SIGPIPE, SIG_IGN);       /* si el cliente muere, write() devuelve EPIPE */

    if (crear_fifo(FIFO_REQ) < 0 || crear_fifo(FIFO_RESP) < 0)
        return 1;

    printf("Servidor FIFO (PID %d) usando %s\n", getpid(), csv);

    int salir = 0;
    while (!salir) {
        printf("Esperando cliente...\n");

        /* open() bloquea hasta que el cliente abre el otro extremo */
        int fd_req = open(FIFO_REQ, O_RDONLY);
        if (fd_req == -1) { perror("open FIFO_REQ"); break; }
        int fd_resp = open(FIFO_RESP, O_WRONLY);
        if (fd_resp == -1) { perror("open FIFO_RESP"); close(fd_req); break; }

        printf("Cliente conectado.\n");

        while (1) {
            char linea[256];
            if (leer_linea(fd_req, linea, sizeof(linea)) < 0) {
                printf("El cliente se desconecto.\n");
                break;
            }

            if (strcmp(linea, "SALIR") == 0) {
                responder(fd_resp, "BYE", 0, "");
                salir = 1;
                break;
            }

            if (strncmp(linea, "CONSULTA|", 9) != 0) {
                if (responder(fd_resp, "ERR", 0, "Mensaje desconocido\n") < 0) break;
                continue;
            }

            char *p = linea + 9;
            char *sep = strchr(p, '|');
            if (sep == NULL) {
                if (responder(fd_resp, "ERR", 0, "Consulta mal formada\n") < 0) break;
                continue;
            }
            *sep = '\0';
            int criterio = atoi(p);
            const char *valor = sep + 1;

            char cuerpo[MAX_CUERPO];
            int n = buscar(csv, criterio, valor, cuerpo, sizeof(cuerpo));
            int r;
            if (n < 0)
                r = responder(fd_resp, "ERR", 0, "El servidor no pudo leer el CSV\n");
            else
                r = responder(fd_resp, "OK", n, cuerpo);
            if (r < 0) {
                perror("write");
                break;
            }
        }

        close(fd_req);
        close(fd_resp);
    }

    unlink(FIFO_REQ);
    unlink(FIFO_RESP);
    printf("Servidor terminado.\n");
    return 0;
}

