#ifndef BUSQUEDA_H
#define BUSQUEDA_H

#include <stddef.h>

#define CSV_PATH        "datos.csv"
#define MAX_RESULTADOS  10      /* maximo registros que se devuelven */
#define MAX_CUERPO      8192    /* tamaño maximo texto de respuesta */
#define MAX_VALOR       128     /* tamaño maximo valor buscado      */

#define CRITERIO_ID     1       /* columna 0: id     */
#define CRITERIO_CIUDAD 2       /* columna 2: ciudad */

/*
 * Lee el CSV linea por linea, nunca lo carga completo.
 * Guarda en 'cuerpo' los registros que coinciden, uno por linea.
 * Retorna: cantidad de coincidencias (0 si no hay), o -1 si no pudo abrir el archivo.
 */
int buscar(const char *archivo, int criterio, const char *valor,
           char *cuerpo, size_t max);

#endif
