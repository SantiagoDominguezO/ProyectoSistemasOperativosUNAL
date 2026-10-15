# Práctica 1 — Procesos, llamadas al sistema e IPC en Linux

**Sistemas Operativos · Universidad Nacional de Colombia**

- **Grupo:** `<NOMBRE DEL GRUPO>`
- **Integrantes:** `Hare Zeyiukuin Atehortua Rincon, `

---

## 1. Descripción del dataset

Archivo `datos.csv`, sintético, generado con `./generar_csv.sh`.

| Columna | Descripción |
|---------|-------------|
| `id` | Entero único (1..N) |
| `nombre` | Nombre + número (ej. `Ana123`) |
| `ciudad` | Una de 10 ciudades colombianas (Bogota, Medellin, Cali, ...) |
| `edad` | Entero entre 18 y 77 |

- **Tamaño por defecto:** 2.000.000 filas (~`<COMPLETAR>` MB; ver `ls -lh datos.csv`).
- El archivo completo permanece en disco: `busqueda.c` lo lee línea por línea con `fgets()` y nunca lo carga en memoria.

**Criterios de búsqueda**

1. **Por ID** (columna 0): devuelve como máximo 1 registro, porque el id es único.
2. **Por ciudad** (columna 2): devuelve hasta `MAX_RESULTADOS` (10) registros. La comparación ignora mayúsculas y minúsculas.

---

## 2. Compilación y ejecución

Requisitos: Linux, `gcc`, `make`.

```bash
make               # compila fifo_server, fifo_client, shm_server, shm_client
make datos         # genera datos.csv (make datos N=5000000 para otro tamaño)
make clean         # borra ejecutables
make limpiar-ipc   # borra FIFO y segmento SHM huérfanos
```

**Versión FIFO** (dos terminales):

```bash
./fifo_server [archivo.csv]    # terminal 1 (por defecto datos.csv)
./fifo_client                  # terminal 2
```

**Versión memoria compartida** (dos terminales):

```bash
./shm_server [archivo.csv]     # terminal 1
./shm_client                   # terminal 2
```

El menú del cliente es igual en ambas versiones:

```
1) Buscar por ID
2) Buscar por ciudad
3) Salir
```

La opción 3 termina el cliente **y** el servidor de forma controlada. Ctrl+C o SIGTERM en el servidor también libera los recursos IPC.

---

## 3. Protocolo de comunicación (FIFO)

Dos FIFO unidireccionales, con cabecera de texto terminada en `\n`:

| FIFO | Dirección |
|------|-----------|
| `/tmp/so_fifo_req` | cliente → servidor |
| `/tmp/so_fifo_resp` | servidor → cliente |

**Cliente → servidor**

```
CONSULTA|<criterio>|<valor>\n     criterio: 1 = ID, 2 = ciudad
SALIR\n                           termina cliente y servidor
```

**Servidor → cliente**

```
<TIPO>|<n>|<len>\n<cuerpo>
```

| Campo | Significado |
|-------|-------------|
| `TIPO` | `OK`, `ERR` o `BYE` |
| `n` | Número de registros encontrados |
| `len` | Número exacto de bytes del cuerpo que sigue |
| `cuerpo` | `len` bytes: registros CSV (uno por línea) o mensaje de error |

Ejemplos:

```
OK|2|38\n<2 líneas CSV>        resultados
OK|0|0\n                       sin resultados
ERR|0|<len>\nMensaje...        error (CSV ilegible, consulta mal formada)
BYE|0|0\n                      confirmación de SALIR
```

**Cómo se evitan escrituras o lecturas parciales**

- `escribir_todo()` repite `write()` hasta enviar todos los bytes.
- `leer_exacto()` repite `read()` hasta recibir exactamente `len` bytes.
- `leer_linea()` lee la cabecera byte a byte hasta `\n`.
- Todas reintentan ante `EINTR`.
- El campo `valor` no puede contener `|`; el cliente lo rechaza.

---

## 4. Sincronización (memoria compartida)

Segmento `/dev/shm/so_practica1_shm`, creado con `shm_open` + `ftruncate` + `mmap(MAP_SHARED)`. La estructura `Compartida` (`shm_comun.h`) contiene dos semáforos POSIX sin nombre (`pshared=1`) más `tipo`, `criterio`, `valor`, `estado`, `n` y `cuerpo`.

| Semáforo | Quién hace `post` | Quién espera |
|----------|-------------------|--------------|
| `sem_solicitud` | Cliente, al terminar de escribir la solicitud | Servidor, dormido en `sem_wait` (sin espera activa) |
| `sem_respuesta` | Servidor, al terminar de escribir la respuesta | Cliente, en `sem_timedwait` (10 s) |

**Flujo:** el cliente escribe la solicitud → `post(sem_solicitud)` → el servidor busca y escribe la respuesta → `post(sem_respuesta)` → el cliente lee. El cliente nunca lee antes de que el servidor termine porque está dormido en `sem_respuesta`.

> `<COMPLETAR>`: si se agrega `sem_mutex` para varios clientes o el drenado de `sem_respuesta`, describirlo aquí. Si no, documentar que se soporta un solo cliente a la vez.

---

## 5. Recursos IPC utilizados

| Versión | Recurso | Creación / liberación |
|---------|---------|-----------------------|
| FIFO | `/tmp/so_fifo_req`, `/tmp/so_fifo_resp` | `mkfifo` por el servidor; `unlink` al terminar |
| SHM | `/dev/shm/so_practica1_shm` | `shm_open` / `shm_unlink` |
| SHM | 2 semáforos POSIX dentro del segmento | `sem_init` / `sem_destroy` |

**Descriptores abiertos por proceso** (completar con `lsof -p <pid>`):

- `fifo_server`: `<COMPLETAR>`
- `fifo_client`: `<COMPLETAR>`

> `ipcs` solo muestra IPC System V. La memoria compartida y los semáforos POSIX se ven con `ls -l /dev/shm` y `lsof`.

---

## 6. Manejo de errores

| Situación | Comportamiento |
|-----------|----------------|
| CSV inexistente | El servidor aborta con `perror()` antes de crear recursos IPC |
| Cliente sin servidor | FIFO: `open(O_NONBLOCK)` da `ENOENT`/`ENXIO`. SHM: `shm_open` da `ENOENT`. Se imprime "El servidor no está en ejecución" |
| Servidor muere con el cliente conectado | FIFO: `read()` devuelve 0 (EOF). SHM: `sem_timedwait` da `ETIMEDOUT` a los 10 s |
| Cliente muere | El servidor FIFO detecta EOF/EPIPE (SIGPIPE ignorado) y espera otro cliente |
| Escrituras parciales, `EINTR` | Manejados en los helpers de `fifo_comun.h` |
| Limpieza | Manejadores de SIGINT/SIGTERM hacen `unlink`/`shm_unlink`. Con `kill -9` los recursos quedan: usar `make limpiar-ipc` |


---

## 7. Observaciones con herramientas de Linux

> Reemplazar con salidas reales de tu máquina (mínimo dos).

### Observación 1 — `strace` del servidor FIFO bloqueado

- **Comando:** `strace -p <pid_servidor>`
- **Salida:**
  ```
  <PEGAR>
  ```
- **Análisis:** el `open()` de lectura sobre la FIFO no retorna hasta que un escritor la abre.

### Observación 2 — estado del proceso bloqueado

- **Comando:** `ps -o pid,ppid,stat,wchan:20,cmd -p <pid>` y `cat /proc/<pid>/status`
- **Salida:**
  ```
  <PEGAR>
  ```
- **Análisis:** `<COMPLETAR>`

### Observación 3 — disco vs. memoria con un CSV grande

- **Comando:** `ls -lh datos.csv` y `grep VmRSS /proc/<pid>/status`
- **Salida:**
  ```
  <PEGAR>
  ```
- **Análisis:** el RSS del servidor se mantiene en KB o pocos MB aunque `datos.csv` pese cientos de MB, porque se lee secuencialmente.

### Observación 4 — descriptores y recursos IPC

- **Comando:** `lsof -p <pid>` y `ls -l /tmp/so_fifo_* /dev/shm`
- **Salida:**
  ```
  <PEGAR>
  ```

---


