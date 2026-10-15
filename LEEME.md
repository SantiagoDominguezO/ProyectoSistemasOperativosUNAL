# Práctica 1 — Procesos, llamadas al sistema e IPC en Linux

**Sistemas Operativos · Universidad Nacional de Colombia**

- **Grupo:** ns
- **Integrantes:** Silvana Ramirez Ardila, Rafael Ricardo Uribe Perdomo, Jose Santiago Dominguez Ortiz, Hare Zeyiukuin Atehortua Rincon

> Los diagramas de arquitectura y de secuencia (FIFO y memoria compartida) se encuentran separados en el archivo [`Diagramas.md`](Diagramas.md).

---

## 1. Descripción del dataset

Archivo `datos.csv`, sintético, generado con `./generar_csv.sh`.

| Columna | Descripción |
|---------|-------------|
| `id` | Entero único (1..N) |
| `nombre` | Nombre + número (ej. `Ana123`) |
| `ciudad` | Una de 10 ciudades colombianas (Bogota, Medellin, Cali, ...) |
| `edad` | Entero entre 18 y 77 |

- **Tamaño por defecto:** 2.000.000 filas (54 MB; ver `ls -lh datos.csv`).
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

**Limitación:** La arquitectura actual soporta un único cliente activo a la vez; no se utiliza `sem_mutex` adicional para concurrencia masiva simultánea.

Los dos semáforos garantizan el orden cliente → servidor → cliente para ese único cliente (nunca lee antes de que el servidor termine de escribir). Si dos clientes escribieran a la vez en `Compartida`, se pisarían `criterio`, `valor` y `cuerpo` (condición de carrera); para evitarlo habría que agregar un `sem_mutex` (valor inicial 1) que cada cliente tome antes de escribir la solicitud y suelte tras leer la respuesta.

---

## 5. Recursos IPC utilizados

| Versión | Recurso | Creación / liberación |
|---------|---------|-----------------------|
| FIFO | `/tmp/so_fifo_req`, `/tmp/so_fifo_resp` | `mkfifo` por el servidor; `unlink` al terminar |
| SHM | `/dev/shm/so_practica1_shm` | `shm_open` / `shm_unlink` |
| SHM | 2 semáforos POSIX dentro del segmento | `sem_init` / `sem_destroy` |

**Descriptores abiertos por proceso** (obtenidos con `lsof -p <pid>`):

- `fifo_server`: `3r` = `so_fifo_req`, `4w` = `so_fifo_resp`, además de los estándar `0` (stdin), `1` (stdout) y `2` (stderr).
- `fifo_client`: `3w` = `so_fifo_req`, `4r` = `so_fifo_resp`, además de los estándar `0` (stdin), `1` (stdout) y `2` (stderr).

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

**Limitaciones conocidas**

- **Un único cliente concurrente a la vez, en SHM y en FIFO.** No hay exclusión entre clientes: en SHM dos clientes se pisarían la estructura compartida (ver sección 4), y en FIFO un segundo cliente podría abrir las tuberías mientras el servidor atiende al primero y las respuestas se mezclarían.
- **Dependencia de limpieza ante cierres abruptos (`kill -9`).** Con `SIGKILL` el servidor no ejecuta sus manejadores, así que `/tmp/so_fifo_req`, `/tmp/so_fifo_resp` y `/dev/shm/so_practica1_shm` quedan en el sistema y hay que borrarlos con `make limpiar-ipc`. Además, en SHM un cliente nuevo no detecta que el servidor murió y espera los 10 s del timeout.
- SHM: si el cliente agota los 10 s, el servidor puede publicar `sem_respuesta` más tarde y el siguiente cliente recibiría una respuesta atrasada.
- Cada consulta reabre y recorre el CSV desde el inicio (búsqueda secuencial), con máximo 10 resultados y un cuerpo de respuesta de hasta 8192 bytes.
- En FIFO el valor buscado no puede contener `|`, porque se usa como separador del protocolo.

---

## 7. Observaciones con herramientas de Linux

> Salidas obtenidas ejecutando los programas con un `datos.csv` de 2.000.000 de filas (~54 MB) en `<COMPLETAR: distribución y versión de Linux>`. Los PID y las horas cambian en cada ejecución.

### Observación 1: `strace` del servidor FIFO

```bash
strace -f -tt -e trace=mknod,mknodat,openat,read,write,close,unlink -o strace_fifo.txt ./fifo_server datos.csv
```

Líneas representativas de `strace_fifo.txt`:

```
296 05:34:59.538689 openat(AT_FDCWD, "datos.csv", O_RDONLY) = 3
296 05:34:59.538923 mknodat(AT_FDCWD, "/tmp/so_fifo_req", S_IFIFO|0666) = 0
296 05:34:59.539034 mknodat(AT_FDCWD, "/tmp/so_fifo_resp", S_IFIFO|0666) = 0
296 05:34:59.539192 openat(AT_FDCWD, "/tmp/so_fifo_req", O_RDONLY) = 3
296 05:35:01.075963 openat(AT_FDCWD, "/tmp/so_fifo_resp", O_WRONLY) = 4
296 05:35:02.077904 openat(AT_FDCWD, "datos.csv", O_RDONLY) = 5
296 05:35:02.482258 write(4, "OK|1|25\n", 8) = 8
296 05:35:03.079987 write(4, "OK|10|195\n", 10) = 10
296 05:35:04.080598 write(4, "BYE|0|0\n", 8) = 8
296 05:35:04.080927 unlink("/tmp/so_fifo_req") = 0
296 05:35:04.081021 unlink("/tmp/so_fifo_resp") = 0
```

**Análisis:**

- `mkfifo()` de la biblioteca se traduce en la llamada al sistema `mknodat` con `S_IFIFO`.
- El `openat(..., O_RDONLY)` sobre `so_fifo_req` empezó en `:59.539` y el siguiente `openat` en `:01.075`. Esa diferencia de ~1,5 s es el tiempo que el servidor estuvo **bloqueado** hasta que el cliente abrió el otro extremo.
- Cada consulta abre `datos.csv` de nuevo (`fd 5`) y responde con un único `write` de cabecera `TIPO|n|len` en el `fd 4`.
- Al terminar, `unlink` elimina las dos FIFO.

### Observación 2: estado de los procesos bloqueados

```bash
ps -o pid,ppid,stat,wchan:20,cmd -p <pid>
grep -E 'State|VmRSS|VmSize' /proc/<pid>/status
```

Servidor FIFO sin cliente conectado:

```
  PID  PPID STAT WCHAN                CMD
  296   292 S    wait_for_partner     ./fifo_server datos.csv
State:  S (sleeping)
VmSize: 2688 kB
VmRSS:  1596 kB
```

Servidor y cliente FIFO con la conexión establecida y esperando:

```
  PID  PPID STAT WCHAN                CMD
  296   292 S    anon_pipe_read       ./fifo_server datos.csv
  307   290 S    anon_pipe_read       ./fifo_client
```

Servidor SHM esperando solicitudes:

```
  PID  PPID STAT WCHAN                CMD
  365   361 S    futex_do_wait        ./shm_server datos.csv
```

**Análisis:**

- Los tres procesos están en estado `S` (dormidos), así que no consumen CPU mientras esperan.
- En FIFO, `wait_for_partner` es el `open()` esperando al otro extremo y `anon_pipe_read` es un `read()` sobre una tubería vacía.
- En memoria compartida, `futex_do_wait` es `sem_wait` dormido en un futex del kernel.
- El PPID de servidor y cliente es distinto: son procesos **no emparentados**, cada uno hijo de su propia shell.

### Observación 3: datos en disco vs. datos en memoria

```bash
ls -lh datos.csv
grep -E 'VmSize|VmRSS|VmHWM' /proc/<pid_servidor>/status
grep -c 'read(5' strace_fifo.txt
```

```
-rw-r--r-- 1 root root 54M datos.csv
VmSize: 2688 kB
VmHWM:  1784 kB
VmRSS:  1784 kB
13641 read(5, "...", 4096) = 4096
```

**Análisis:** el servidor ya había recorrido el archivo completo (búsqueda del id 1999999), pero su memoria residente máxima fue de solo ~1,7 MB frente a 54 MB de CSV. Se hicieron 13.641 `read()` de 4096 bytes (≈53 MiB), es decir, el archivo se procesa por bloques y nunca se carga completo.

### Observación 4: descriptores abiertos y recursos IPC

```bash
lsof -p <pid_servidor>,<pid_cliente> | grep -i fifo
ls -l /tmp/so_fifo_* /dev/shm/so_practica1_shm
```

```
fifo_serv 296 root  3r  FIFO  254,0  0t0  359 /tmp/so_fifo_req
fifo_serv 296 root  4w  FIFO  254,0  0t0  358 /tmp/so_fifo_resp
fifo_clie 307 root  3w  FIFO  254,0  0t0  359 /tmp/so_fifo_req
fifo_clie 307 root  4r  FIFO  254,0  0t0  358 /tmp/so_fifo_resp

-rw-r--r-- 1 root root 8400 /dev/shm/so_practica1_shm
```

**Análisis:**

- Cada FIFO tiene el mismo número de inodo en servidor y cliente (359 y 358), y los extremos son opuestos: lo que uno abre para escribir, el otro lo abre para leer.
- En SHM el segmento ocupa 8400 bytes, que es `sizeof(Compartida)`, y es un archivo visible en `/dev/shm`.
- `ipcs -m -s` sale **vacío** porque solo lista IPC System V; la memoria compartida y los semáforos POSIX no aparecen ahí.

### Observación 5: terminación inesperada (`kill -9`)

| Caso | Recursos que quedan | Efecto en el cliente |
|------|---------------------|----------------------|
| `kill -9` al servidor FIFO | `/tmp/so_fifo_req` y `/tmp/so_fifo_resp` siguen existiendo | Al iniciar: `open(O_NONBLOCK)` da `ENXIO` y imprime "El servidor no esta en ejecucion." |
| `kill -9` al servidor SHM | `/dev/shm/so_practica1_shm` sigue existiendo | `shm_open` funciona, el cliente espera y a los 10 s imprime "El servidor no respondio en 10 segundos." |

Se limpian con `make limpiar-ipc`. Un servidor nuevo los reutiliza o recrea (`EEXIST` se ignora en FIFO y `shm_unlink` previo en SHM).

---


