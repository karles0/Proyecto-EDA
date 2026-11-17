"""Genera los graficos del benchmark a partir de los CSV de ./benchmark.

Uso:  python3 herramientas/graficar.py [carpeta_resultados]
      (por defecto: resultados/; los PNG se guardan en resultados/graficos/)

Requiere solo matplotlib (ver herramientas/requirements.txt).
"""
import collections
import csv
import os
import statistics as st
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt              

COLOR = {"D-tree": "#1f4e9e", "HKS": "#12806e"}
ESTRUCTURAS = ["D-tree", "HKS"]
OPERACIONES = [("eliminar", "Eliminar línea"), ("consultar", "Consultar conectividad"), ("insertar", "Restaurar línea")]
TIPOS = [("fuera", "Fuera del árbol"), ("reemplazo", "Con reemplazo"), ("isla", "Islanding (sin reemplazo)")]

plt.rcParams.update({
    "figure.dpi": 130, "savefig.dpi": 160, "font.size": 10, "axes.titlesize": 11,
    "axes.spines.top": False, "axes.spines.right": False, "axes.grid": True,
    "grid.alpha": 0.3, "legend.frameon": False,
})

def leer(ruta):
    with open(ruta, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))

def percentil(xs, p):
    xs = sorted(xs)
    return xs[int(p * (len(xs) - 1))]

def main(carpeta):
    salida = os.path.join(carpeta, "graficos")
    os.makedirs(salida, exist_ok=True)
    top = leer(os.path.join(carpeta, "topologia.csv"))
    casos = [t["caso"] for t in sorted(top, key=lambda t: int(t["n"]))]
    n_de = {t["caso"]: int(t["n"]) for t in top}
    resumen = leer(os.path.join(carpeta, "resumen.csv"))
    construccion = leer(os.path.join(carpeta, "construccion.csv"))
    escenarios = leer(os.path.join(carpeta, "escenarios.csv"))
    cortes = leer(os.path.join(carpeta, "cortes_n1.csv"))

    rep = collections.defaultdict(list)
    for f in resumen:
        rep[(f["caso"], f["estructura"], f["operacion"])].append(f)
    def med(caso, est, op, campo):
        return st.median(float(f[campo]) for f in rep[(caso, est, op)])
    def rango(caso, est, op, campo):
        v = [float(f[campo]) for f in rep[(caso, est, op)]]
        return min(v), max(v)

    x = list(range(len(casos)))
    ancho = 0.38

    fig, ax = plt.subplots(1, 3, figsize=(13, 3.8))
    ax[0].bar(x, [float(t["m_sobre_n"]) for t in sorted(top, key=lambda t: int(t["n"]))], color="#6f7f8d")
    ax[0].axhline(1.0, color="#c8156f", lw=1, ls="--")
    ax[0].text(-0.4, 1.62, "línea punteada: un árbol tiene m/n ≈ 1", color="#c8156f", ha="left", va="bottom", fontsize=9)
    ax[0].set_title("Densidad m/n: todas las redes son casi árboles")
    ax[0].set_ylim(0, 1.8)
    ax[1].bar(x, [float(t["porcentaje_puentes"]) for t in sorted(top, key=lambda t: int(t["n"]))], color="#6f7f8d")
    ax[1].set_title("Líneas puente (% del total)")
    ax[1].set_ylabel("%")
    ax2 = ax[1].twinx()
    ax2.plot(x, [int(t["diametro"]) for t in sorted(top, key=lambda t: int(t["n"]))], "o-", color="#c8156f")
    ax2.set_ylabel("diámetro", color="#c8156f")
    ax2.spines["right"].set_visible(True)
    ax2.grid(False)
    por_caso = collections.defaultdict(list)
    for f in cortes:
        por_caso[f["caso"]].append(int(f["lado_menor"]))
    for c in casos:
        v = sorted(por_caso[c])
        if v:
            ax[2].step(v, [(i + 1) / len(v) for i in range(len(v))], where="post", label=c)
    ax[2].set_xscale("log")
    ax[2].set_title("N-1: barras que aísla cada puente (FDA)")
    ax[2].set_xlabel("tamaño del lado menor (barras)")
    ax[2].set_ylabel("fracción de puentes")
    ax[2].legend(fontsize=8)
    for a in ax[:2]:
        a.set_xticks(x, casos, rotation=20)
    fig.tight_layout()
    fig.savefig(os.path.join(salida, "1_topologia.png"))
    plt.close(fig)

    fig, ax = plt.subplots(1, 3, figsize=(13, 3.9))
    for k, (op, titulo) in enumerate(OPERACIONES):
        for j, e in enumerate(ESTRUCTURAS):
            y = [med(c, e, op, "media_us") for c in casos]
            lo_hi = [rango(c, e, op, "media_us") for c in casos]
            err = [[yy - lo for yy, (lo, _) in zip(y, lo_hi)], [hi - yy for yy, (_, hi) in zip(y, lo_hi)]]
            pos = [i + (j - 0.5) * ancho for i in x]
            ax[k].bar(pos, y, ancho, color=COLOR[e], label=e, yerr=err, capsize=2, error_kw={"lw": 0.8})
        ax[k].set_title(titulo)
        ax[k].set_xticks(x, casos, rotation=20)
        ax[k].set_ylabel("tiempo medio por operación (µs)")
    ax[0].legend()
    fig.suptitle("Contingencias N-2: tiempo medio por operación (mediana de 5 repeticiones; barras de error = mín–máx)", fontsize=10)
    fig.tight_layout()
    fig.savefig(os.path.join(salida, "2_tiempo_por_operacion.png"))
    plt.close(fig)

    fig, ax = plt.subplots(1, 2, figsize=(11, 4))
    ns = [n_de[c] for c in casos]
    for e in ESTRUCTURAS:
        ax[0].plot(ns, [med(c, e, "eliminar", "media_us") for c in casos], "o-", color=COLOR[e], label=f"{e}: media")
        ax[0].plot(ns, [med(c, e, "eliminar", "p99_us") for c in casos], "s--", color=COLOR[e], alpha=0.7, label=f"{e}: percentil 99")
    ax[0].set_xscale("log"); ax[0].set_yscale("log")
    ax[0].set_title("Eliminar línea: crecimiento con el tamaño de la red")
    ax[0].set_xlabel("barras (n)"); ax[0].set_ylabel("µs")
    ax[0].legend(fontsize=8)
    razon_media = [med(c, "HKS", "eliminar", "media_us") / med(c, "D-tree", "eliminar", "media_us") for c in casos]
    razon_p99 = [med(c, "HKS", "eliminar", "p99_us") / med(c, "D-tree", "eliminar", "p99_us") for c in casos]
    ax[1].plot(ns, razon_media, "o-", color="#17222d", label="media")
    ax[1].plot(ns, razon_p99, "s--", color="#c8156f", label="percentil 99")
    ax[1].axhline(1.0, color="#6f7f8d", lw=1)
    ax[1].text(ns[0], 1.08, "por debajo de 1, HKS sería más rápido", fontsize=8, color="#5a6875")
    ax[1].set_xscale("log")
    ax[1].set_ylim(0, max(razon_media) * 1.15)
    ax[1].set_title("Cuántas veces más lento es HKS al eliminar")
    ax[1].set_xlabel("barras (n)"); ax[1].set_ylabel("tiempo HKS / tiempo D-tree")
    ax[1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(os.path.join(salida, "3_escalabilidad.png"))
    plt.close(fig)

    tipo_t = collections.defaultdict(list)
    for f in escenarios:
        for k in ("1", "2"):
            arbol = f["arbol" + k] == "1"
            isla = f["isla_e" + k] == "1"
            tipo = "fuera" if not arbol else ("isla" if isla else "reemplazo")
            tipo_t[(f["caso"], f["estructura"], tipo)].append(float(f[f"t_elim{k}_us"]))
    fig, ax = plt.subplots(1, 3, figsize=(13, 3.9), sharey=True)
    for k, (tipo, titulo) in enumerate(TIPOS):
        for j, e in enumerate(ESTRUCTURAS):
            y = [st.median(tipo_t[(c, e, tipo)]) for c in casos]
            p99 = [percentil(tipo_t[(c, e, tipo)], 0.99) for c in casos]
            pos = [i + (j - 0.5) * ancho for i in x]
            ax[k].bar(pos, y, ancho, color=COLOR[e], label=f"{e} (mediana)")
            ax[k].scatter(pos, p99, marker="_", s=120, color=COLOR[e], zorder=3,
                          label=f"{e} (percentil 99)" if k == 0 else None)
        ax[k].set_yscale("log")
        ax[k].set_title(titulo)
        ax[k].set_xticks(x, casos, rotation=20)
    ax[0].set_ylabel("µs por eliminación")
    ax[0].legend(fontsize=8)
    fig.suptitle("Eliminar línea según el resultado de la búsqueda de reemplazo (repetición 1)", fontsize=10)
    fig.tight_layout()
    fig.savefig(os.path.join(salida, "4_eliminacion_por_tipo.png"))
    plt.close(fig)

    elim = collections.defaultdict(list)
    for f in escenarios:
        elim[(f["caso"], f["estructura"])] += [float(f["t_elim1_us"]), float(f["t_elim2_us"])]
    fig, ax = plt.subplots(1, len(casos), figsize=(3.0 * len(casos), 3.6), sharey=True)
    for k, c in enumerate(casos):
        for e in ESTRUCTURAS:
            v = sorted(elim[(c, e)])
            ax[k].plot(v, [1 - (i + 1) / len(v) for i in range(len(v))], color=COLOR[e], label=e)
        ax[k].set_xscale("log"); ax[k].set_yscale("log")
        ax[k].set_ylim(1e-4, 1.2)
        ax[k].set_title(c)
        ax[k].set_xlabel("µs")
    ax[0].set_ylabel("fracción de eliminaciones\nque tardan más que x")
    ax[0].legend(fontsize=8)
    fig.suptitle("Cola de la distribución de tiempos al eliminar (más a la derecha = más lento)", fontsize=10)
    fig.tight_layout()
    fig.savefig(os.path.join(salida, "5_distribucion_eliminacion.png"))
    plt.close(fig)

    cons = collections.defaultdict(list)
    for f in construccion:
        cons[(f["caso"], f["estructura"])].append(f)
    fig, ax = plt.subplots(1, 2, figsize=(11, 3.9))
    for j, e in enumerate(ESTRUCTURAS):
        pos = [i + (j - 0.5) * ancho for i in x]
        ax[0].bar(pos, [int(cons[(c, e)][0]["memoria_bytes"]) / n_de[c] for c in casos], ancho, color=COLOR[e], label=e)
        ax[1].bar(pos, [st.median(float(f["construccion_ms"]) for f in cons[(c, e)]) for c in casos], ancho, color=COLOR[e], label=e)
    ax[0].set_title("Memoria estimada por barra")
    ax[0].set_ylabel("bytes / barra")
    ax[1].set_title("Construcción (insertar todas las líneas)")
    ax[1].set_ylabel("ms (mediana de 5 repeticiones)")
    ax[1].set_yscale("log")
    for a in ax:
        a.set_xticks(x, casos, rotation=20)
        a.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(salida, "6_memoria_y_construccion.png"))
    plt.close(fig)

    lineas = ["| Caso | n | Operación | D-tree media (µs) | HKS media (µs) | HKS / D-tree | D-tree p99 (µs) | HKS p99 (µs) |",
              "|---|---|---|---|---|---|---|---|"]
    for c in casos:
        for op, titulo in OPERACIONES:
            d, h = med(c, "D-tree", op, "media_us"), med(c, "HKS", op, "media_us")
            lineas.append(f"| {c} | {n_de[c]} | {titulo} | {d:.3f} | {h:.3f} | {h / d:.2f} | "
                          f"{med(c, 'D-tree', op, 'p99_us'):.2f} | {med(c, 'HKS', op, 'p99_us'):.2f} |")
    with open(os.path.join(carpeta, "tabla_resumen.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lineas) + "\n")

    print(f"Gráficos guardados en {salida}/ y tabla en {carpeta}/tabla_resumen.md")
    for nombre in sorted(os.listdir(salida)):
        print("  ", nombre)

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "resultados")
