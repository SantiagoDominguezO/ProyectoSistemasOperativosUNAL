#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "busqueda.h"

/* Copia en 'dest' el campo numero 'idx' (empezando en 0) de una linea CSV. */
static int obtener_campo(const char *linea, int idx, char *dest, size_t max)
{
    const char *p = linea;
    for (int actual = 0; actual < idx; actual++) {
        p = strchr(p, ',');
        if (p == NULL)
            return -1;
        p++;
    }
    size_t i = 0;
    while (*p != '\0' && *p != ',' && *p != '\n' && *p != '\r' && i < max - 1)
        dest[i++] = *p++;
    dest[i] = '\0';
    return 0;
}

int buscar(const char *archivo, int criterio, const char *valor,
           char *cuerpo, size_t max)
{
    FILE *f = fopen(archivo, "r");
    if (f == NULL)
        return -1;

    char linea[1024];
    char campo[256];
    size_t usado = 0;
    int n = 0;
    int columna = (criterio == CRITERIO_ID) ? 0 : 2;

    cuerpo[0] = '\0';
    fgets(linea, sizeof(linea), f);          /* saltar el encabezado */

    while (fgets(linea, sizeof(linea), f) != NULL) {
        if (obtener_campo(linea, columna, campo, sizeof(campo)) != 0)
            continue;
        if (strcasecmp(campo, valor) != 0)
            continue;

        linea[strcspn(linea, "\r\n")] = '\0';
        int w = snprintf(cuerpo + usado, max - usado, "%s\n", linea);
        if (w < 0 || (size_t)w >= max - usado) {
            cuerpo[usado] = '\0';
            break;
        }
        usado += (size_t)w;
        n++;

        if (criterio == CRITERIO_ID || n >= MAX_RESULTADOS)
            break;                            /* el id es unico / limite */
    }

    fclose(f);
    return n;
}
