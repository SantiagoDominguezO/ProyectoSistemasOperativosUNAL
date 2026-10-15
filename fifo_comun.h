#ifndef FIFO_COMUN_H
#define FIFO_COMUN_H

#include <errno.h>
#include <unistd.h>
#include <stddef.h>

#define FIFO_REQ  "/tmp/so_fifo_req"    /* cliente -> servidor */
#define FIFO_RESP "/tmp/so_fifo_resp"   /* servidor -> cliente */

/* Escribe exactamente n bytes (repite write() si fue parcial). */
static inline int escribir_todo(int fd, const char *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t w = write(fd, buf + total, n - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)w;
    }
    return 0;
}

/* Lee exactamente n bytes. Retorna -1 si hay error o si el otro extremo cerro. */
static inline int leer_exacto(int fd, char *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, buf + total, n - total);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1;            /* EOF: el otro proceso cerro */
        total += (size_t)r;
    }
    return 0;
}

/* Lee hasta '\n' (sin incluirlo). Retorna -1 en error o EOF. */
static inline int leer_linea(int fd, char *buf, size_t max)
{
    size_t i = 0;
    while (i < max - 1) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i;
}

#endif

