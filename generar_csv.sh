#!/bin/bash
# Genera un CSV de prueba: ./generar_csv.sh [num_filas]   (por defecto 2 millones)
N=${1:-2000000}
awk -v n="$N" 'BEGIN {
    srand(42)
    split("Bogota,Medellin,Cali,Barranquilla,Cartagena,Bucaramanga,Pereira,Manizales,Cucuta,Ibague", c, ",")
    split("Ana,Luis,Maria,Carlos,Sofia,Juan,Laura,Pedro,Camila,Diego", nm, ",")
    print "id,nombre,ciudad,edad"
    for (i = 1; i <= n; i++)
        printf "%d,%s%d,%s,%d\n", i, nm[int(rand()*10)+1], i%1000, c[int(rand()*10)+1], 18+int(rand()*60)
}' > datos.csv
echo "datos.csv generado con $N filas"
