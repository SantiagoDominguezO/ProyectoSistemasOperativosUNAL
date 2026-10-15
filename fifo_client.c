#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include "busqueda.h"
#include "fifo_comun.h"

/* Envia una consulta y muestra la respuesta. Retorna -1 si se perdio el servidor. */
static int consultar(int fd_req, int fd_resp, int criterio, const char *valor)
{
    char msg[256];
    snprintf(msg, sizeof(msg), "CONSULTA|%d|%s\n", criterio, valor);
    if (escribir_todo(fd_req, msg, strlen(msg)) < 0) {
        perror("write");
        return -1;
    }

    char cab[64];
    if (leer_linea(fd_resp, cab, sizeof(cab)) < 0) {
        printf("El servidor dejo de responder.\n");
        return -1;
    }

    char tipo[8];
    int n, len;
    if (sscanf(cab, "%7[^|]|%d|%d", tipo, &n, &len) != 3 || len < 0 || len >= MAX_CUERPO) {
        printf("Respuesta invalida del servidor.\n");
        return -1;
    }

    char cuerpo[MAX_CUERPO];
    if (len > 0 && leer_exacto(fd_resp, cuerpo, (size_t)len) < 0) {
        printf("Respuesta incompleta.\n");
        return -1;
    }
    cuerpo[len] = '\0';

    if (strcmp(tipo, "ERR") == 0)
        printf("Error del servidor: %s", cuerpo);
    else if (n == 0)
        printf("No se encontraron resultados.\n");
    else
        printf("Resultados (%d):\n%s", n, cuerpo);
    return 0;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    /* O_NONBLOCK: si no hay servidor leyendo, falla con ENXIO en vez de bloquearse */
    int fd_req = open(FIFO_REQ, O_WRONLY | O_NONBLOCK);
    if (fd_req == -1) {
        if (errno == ENOENT || errno == ENXIO)
            printf("El servidor no esta en ejecucion.\n");
        else
            perror("open FIFO_REQ");
        return 1;
    }

    int fd_resp = open(FIFO_RESP, O_RDONLY);   /* bloquea hasta que el servidor abre */
    if (fd_resp == -1) {
        perror("open FIFO_RESP");
        close(fd_req);
        return 1;
    }

    while (1) {
        printf("\n1) Buscar por ID\n2) Buscar por ciudad\n3) Salir\nOpcion: ");
        char entrada[64];
        if (fgets(entrada, sizeof(entrada), stdin) == NULL)
            break;
        int op = atoi(entrada);

        if (op == 3) {
            escribir_todo(fd_req, "SALIR\n", 6);
            char cab[64];
            leer_linea(fd_resp, cab, sizeof(cab));   /* espera el BYE */
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
        if (strchr(valor, '|') != NULL || valor[0] == '\0') {
            printf("Valor invalido.\n");
            continue;
        }

        if (consultar(fd_req, fd_resp, op, valor) < 0)
            break;
    }

    close(fd_req);
    close(fd_resp);
    return 0;
}

