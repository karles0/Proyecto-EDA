"""Extrae las coordenadas de las barras del archivo PowerWorld (.aux) de ACTIVSg2000.

Genera data/ACTIVSg2000_barras.csv con: barra, nombre, kv, subestacion, lat, lon.
Solo usa la biblioteca estandar de Python.

Uso: python3 herramientas/extraer_coordenadas.py ruta/ACTIVSg2000.aux data/ACTIVSg2000_barras.csv
"""
import csv
import shlex
import sys

def leer_seccion(lineas, nombre):
    """Devuelve (campos, filas) de la seccion 'DATA (<nombre>, [...]) { ... }'."""
    i = 0
    while i < len(lineas) and not lineas[i].startswith(f"DATA ({nombre},"):
        i += 1
    if i == len(lineas):
        raise ValueError(f"No se encontro la seccion {nombre}")
    encabezado = ""
    while "{" not in lineas[i]:
        encabezado += lineas[i]
        i += 1
    campos = [c.strip() for c in encabezado.split("[", 1)[1].rsplit("]", 1)[0].split(",")]
    filas = []
    i += 1
    while not lineas[i].startswith("}"):
        if lineas[i].strip():
            filas.append(shlex.split(lineas[i]))
        i += 1
    return campos, filas

def main(ruta_aux, ruta_csv):
    with open(ruta_aux, encoding="latin-1") as f:
        lineas = [l.rstrip("\r\n") for l in f]
    campos, filas = leer_seccion(lineas, "Bus")
    idx = {c: k for k, c in enumerate(campos)}
    with open(ruta_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["barra", "nombre", "kv", "subestacion", "lat", "lon"])
        for fila in filas:
            w.writerow([
                int(fila[idx["BusNum"]]),
                fila[idx["BusName"]].strip(),
                float(fila[idx["BusNomVolt"]]),
                int(fila[idx["SubNum"]]),
                float(fila[idx["Latitude:1"]]),
                float(fila[idx["Longitude:1"]]),
            ])
    print(f"{len(filas)} barras escritas en {ruta_csv}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
