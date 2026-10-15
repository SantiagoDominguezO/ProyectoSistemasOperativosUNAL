PRACTICA 1 - Procesos, llamadas al sistema e IPC en Linux
Sistemas Operativos - Universidad Nacional de Colombia
Grupo: <NOMBRE DEL GRUPO>
Integrantes: <NOMBRES>

======================================================================
1. DESCRIPCION DEL DATASET
======================================================================
Archivo: datos.csv (sintetico, generado con ./generar_csv.sh)
Columnas: id,nombre,ciudad,edad
  - id      : 1011090191, 
  - nombre  : Hare Zeyiukuin Atehortua Rincon.
  - ciudad  : Bogota D.C
Tamano por defecto: 2.000.000 filas (~<COMPLETAR> MB; ver `ls -lh datos.csv`).
El archivo completo permanece en disco: busqueda.c lo lee linea por linea
con fgets() y nunca lo carga en memoria.

Criterios de busqueda:
  1) Por ID     (columna 0): devuelve como maximo 1 registro (el id es unico).
  2) Por ciudad (columna 2): devuelve hasta MAX_RESULTADOS (10) registros.
     La comparacion ignora mayusculas/minusculas.

======================================================================
2. COMPILACION Y EJECUCION
======================================================================
Requisitos: Linux, gcc, make.

  make                # compila fifo_server, fifo_client, shm_server, shm_client
  make datos          # genera datos.csv (make datos N=5000000 para otro tamano)
  make clean          # borra ejecutables
  make limpiar-ipc    # borra FIFO y segmento SHM huerfanos

Version FIFO (dos terminales):
  Terminal 1:  ./fifo_server [archivo.csv]     (por defecto datos.csv)
  Terminal 2:  ./fifo_client

Version memoria compartida (dos terminales):
  Terminal 1:  ./shm_server [archivo.csv]
  Terminal 2:  ./shm_client

El menu del cliente es igual en ambas versiones:
  1) Buscar por ID   2) Buscar por ciudad   3) Salir
La opcion 3 termina el cliente Y el servidor de forma controlada.
Ctrl+C / SIGTERM en el servidor tambien libera los recursos IPC.

======================================================================
3. PROTOCOLO DE COMUNICACION (FIFO)
======================================================================
Dos FIFO unidireccionales (texto, una cabecera por linea):
  /tmp/so_fifo_req   cliente -> servidor
  /tmp/so_fifo_resp  servidor -> cliente

Cliente -> servidor (termina en '\n'):
  CONSULTA|<criterio>|<valor>\n     criterio: 1 = ID, 2 = ciudad
  SALIR\n                           pide terminar cliente y servidor

Servidor -> cliente:
  <TIPO>|<n>|<len>\n<cuerpo>
    TIPO   : OK | ERR | BYE
    n      : numero de registros encontrados
    len    : numero exacto de bytes del cuerpo que sigue
    cuerpo : len bytes (registros CSV, uno por linea, o mensaje de error)

Ejemplos:
  OK|2|38\n<2 lineas CSV>      resultados
  OK|0|0\n                     sin resultados
  ERR|0|<len>\nMensaje...      error (CSV ilegible, consulta mal formada)
  BYE|0|0\n                    confirmacion de SALIR

Evitar escrituras/lecturas parciales: escribir_todo() repite write() hasta
enviar todo; leer_exacto() repite read() hasta recibir exactamente len bytes;
leer_linea() lee la cabecera byte a byte hasta '\n'. Se reintenta en EINTR.
El campo valor no puede contener '|' (el cliente lo rechaza).

======================================================================
4. SINCRONIZACION Y PROTOCOLO (MEMORIA COMPARTIDA)
======================================================================
Segmento: /dev/shm/so_practica1_shm (shm_open + ftruncate + mmap MAP_SHARED).
Estructura Compartida (shm_comun.h): dos semaforos POSIX sin nombre
(pshared=1) + tipo, criterio, valor, estado, n, cuerpo.

  sem_solicitud : el cliente hace sem_post cuando la solicitud esta escrita;
                  el servidor duerme en sem_wait (sin espera activa).
  sem_respuesta : el servidor hace sem_post cuando termino de escribir la
                  respuesta; el cliente duerme en sem_timedwait (10 s).

Flujo: cliente escribe solicitud -> post(solicitud) -> servidor busca y
escribe respuesta -> post(respuesta) -> cliente lee. El cliente nunca lee
antes de que el servidor termine porque esta dormido en sem_respuesta.
<COMPLETAR si se agrega sem_mutex para varios clientes / drenado de
sem_respuesta, y si no, documentar que se soporta un solo cliente.>

======================================================================
5. RECURSOS IPC UTILIZADOS
======================================================================
FIFO:  /tmp/so_fifo_req, /tmp/so_fifo_resp (creadas con mkfifo por el
       servidor, borradas con unlink al terminar).
SHM:   /dev/shm/so_practica1_shm (shm_open/shm_unlink) y 2 semaforos
       POSIX dentro del segmento (sem_init/sem_destroy).
Descriptores por proceso (completar con `lsof -p <pid>`):
  fifo_server : <COMPLETAR>
  fifo_client : <COMPLETAR>
Nota: `ipcs` solo muestra IPC System V; la SHM y semaforos POSIX se ven con
`ls -l /dev/shm` y `lsof`.

======================================================================
6. MANEJO DE ERRORES
======================================================================
- CSV inexistente: el servidor aborta con perror() antes de crear IPC.
- Cliente sin servidor: FIFO -> open(O_NONBLOCK) da ENOENT/ENXIO; SHM ->
  shm_open da ENOENT. Se imprime "El servidor no esta en ejecucion".
- Servidor muerto con el cliente conectado: read() devuelve 0 (EOF) en FIFO;
  en SHM sem_timedwait devuelve ETIMEDOUT tras 10 s.
- Cliente muerto: el servidor FIFO detecta EOF/EPIPE (SIGPIPE ignorado) y
  vuelve a esperar otro cliente.
- Escrituras parciales y EINTR: manejados en los helpers de fifo_comun.h.
- Limpieza: manejadores de SIGINT/SIGTERM hacen unlink/shm_unlink.
  Con kill -9 los recursos quedan; limpiar con `make limpiar-ipc`.
- Limitaciones conocidas: <COMPLETAR (ej. un solo cliente a la vez)>.

======================================================================
7. OBSERVACIONES CON HERRAMIENTAS DE LINUX
======================================================================
(Reemplazar con salidas reales de TU maquina; minimo dos.)

Observacion 1 - strace del servidor FIFO bloqueado:
  Comando : strace -p <pid_servidor>
  Salida  : <PEGAR> (ej. open("/tmp/so_fifo_req", O_RDONLY) sin retornar)
  Analisis: el open() de lectura se bloquea hasta que un escritor abre la FIFO.

Observacion 2 - estado del proceso bloqueado:
  Comando : ps -o pid,ppid,stat,wchan:20,cmd -p <pid> ; cat /proc/<pid>/status
  Salida  : <PEGAR> (estado S, wchan, etc.)
  Analisis: <COMPLETAR>

Observacion 3 - disco vs memoria con CSV grande:
  Comando : ls -lh datos.csv ; grep VmRSS /proc/<pid>/status
  Salida  : <PEGAR>
  Analisis: el RSS del servidor es de KB/pocos MB aunque datos.csv pese
            cientos de MB, porque se lee secuencialmente.

Observacion 4 - descriptores y recursos IPC:
  Comando : lsof -p <pid> ; ls -l /tmp/so_fifo_* /dev/shm
  Salida  : <PEGAR>

