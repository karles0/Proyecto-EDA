# Conectividad dinámica en redes eléctricas: D-tree y Euler Tour Tree HKS

Implementaciones en C++17 del **D-tree** y del **Euler Tour Tree HKS** (Henzinger–King
simplificado, sin niveles), conectadas al caso de red eléctrica **ACTIVSg2000** del
Electric Grid Test Case Repository de Texas A&M.

## Estructura

```
conectividad_dinamica/
├── Makefile
├── README.md
├── data/
│   ├── ACTIVSg2000.m              Caso MATPOWER original (barras y ramas)
│   ├── ACTIVSg2000_barras.csv     Coordenadas geográficas de cada barra (para el mapa)
│   └── ACTIVSg2000_aristas.txt    Grafo simple derivado (lo regenera ./principal)
│   └── casos/                     Casos MATPOWER para el benchmark (IEEE 118, IEEE 300,
│                                  ACTIVSg10k, ACTIVSg25k) y su licencia
├── herramientas/
│   ├── extraer_coordenadas.py     Genera el CSV de coordenadas desde el archivo .aux original
│   ├── graficar.py                Genera los gráficos del benchmark
│   └── requirements.txt           Dependencia de Python (matplotlib)
├── resultados/                    CSV y gráficos de la corrida de benchmark incluida
├── terceros/
│   ├── httplib.h                  cpp-httplib v0.18.3 (servidor HTTP, licencia MIT)
│   └── LICENSE_httplib
├── web/
│   └── index.html                 Interfaz gráfica (mapa interactivo)
└── src/
    ├── adyacencia.hpp             Conjunto de aristas no-árbol (común a ambas estructuras)
    ├── cargador_matpower.hpp/.cpp Lectura del formato MATPOWER y conversión a grafo
    ├── dtree.hpp/.cpp             D-tree
    ├── ett_hks.hpp/.cpp           Euler Tour Tree HKS
    ├── main.cpp                   Carga la red, construye ambas estructuras y corre una contingencia N-2
    ├── mi_prueba.cpp              Caso pequeño verificable a mano (5 vértices)
    ├── servidor.cpp               Servidor de la interfaz gráfica
    ├── benchmark.cpp              Benchmark de contingencias N-2
    └── verificar.cpp              Verificación de corrección contra un oráculo (depuración)
```

## Compilar y ejecutar

Requiere g++ (o clang++) con soporte de C++17.

```bash
make            # compila ./principal, ./verificar, ./mi_prueba, ./servidor y ./benchmark
./principal     # usa data/ACTIVSg2000.m (o: ./principal ruta/a/otro_caso.m)
./verificar     # verificación de corrección (no es el benchmark)
./mi_prueba     # caso pequeño con resultado conocido
```

En Windows sin `make`:

```bash
g++ -std=c++17 -O2 src/main.cpp src/dtree.cpp src/ett_hks.cpp src/cargador_matpower.cpp -o principal
```

Para los benchmarks conviene compilar sin aserciones:
`make CXXFLAGS="-std=c++17 -O2 -DNDEBUG"`.

## Interfaz gráfica

```bash
make
./servidor            # luego abrir http://localhost:8080 en el navegador
```

Opciones: `./servidor --puerto 8081` si el 8080 está ocupado. Se detiene con `Ctrl+C`.

### Cómo funciona

```
Navegador (web/index.html)  ──HTTP/JSON──►  ./servidor (C++)  ──►  DTree + EulerTourTreeHKS
       mapa, clics, panel        ◄──────────   respuestas          + verificación por BFS
```

El servidor carga la red, construye **ambas estructuras** y las mantiene en memoria. Cada
acción de la interfaz se ejecuta sobre las dos y su resultado se contrasta con un cálculo
por BFS desde cero; el panel muestra si coinciden.

- **Mapa:** barras ubicadas por sus coordenadas reales (Texas) y líneas coloreadas por
  nivel de tensión. Las barras de una misma subestación se reparten en un círculo pequeño.
  Se puede mover el mapa arrastrando y hacer zoom con la rueda del ratón.
- **Hacer caer líneas:** clic en una línea para hacerla caer (`eliminar`); otro clic la
  restaura (`insertar`). La tabla de la última operación indica, para cada estructura, si
  la línea estaba en su árbol de expansión y si encontró una arista de reemplazo o hubo
  islanding, con el tiempo de la operación. Los tiempos son referenciales (una sola
  operación); no reemplazan al benchmark.
- **Consultar conectividad:** clic en dos barras para ejecutar `conectados(u, v)` en ambas
  estructuras.
- **Escenarios:** caída aleatoria, caída de una línea puente (aísla una zona con seguridad),
  el ejemplo N-2 (6173–6265 y 6092–6340, que aísla 11 barras; primero restaura la red) y
  restaurar la red completa.
- **Resaltar líneas puente:** calculadas con el algoritmo de Tarjan sobre la red actual.
- Las barras aisladas se marcan en magenta con un halo, y el botón *Acercar a la zona
  aislada* centra el mapa en ellas.

API (todas devuelven JSON): `GET /api/red`, `GET /api/estado`, `POST /api/caer?i=`,
`POST /api/restaurar?i=`, `POST /api/restaurar_todo`, `GET /api/conn?u=&v=`,
`GET /api/puentes`. Los índices de línea y de barra son los del archivo cargado.

La tipografía se descarga de Google Fonts; sin conexión a internet la interfaz usa una
fuente del sistema y funciona igual.

## Benchmarks

### Cómo correrlos y graficar

```bash
make benchmark
./benchmark                                   # ~1 minuto; escribe en resultados/
python3 -m venv .venv                         # entorno de Python para los gráficos
source .venv/bin/activate
pip install -r herramientas/requirements.txt  # instala matplotlib
python3 herramientas/graficar.py              # escribe resultados/graficos/*.png
deactivate
```

Opciones: `./benchmark --escenarios 20000 --repeticiones 5 --consultas 8 --semilla 2026
--salida resultados [casos.m ...]`. Sin casos, usa los cinco de `data/`.

### Carga de trabajo y metodología

Cada **escenario** es una contingencia N-2, igual que en el análisis de contingencias:
`eliminar(e1)`, `eliminar(e2)`, 10 consultas `conectados` (los extremos de las dos líneas
caídas, que detectan el islanding, más 8 pares al azar), `insertar(e2)` e `insertar(e1)`.

- **Casos:** IEEE 118, IEEE 300, ACTIVSg2000, ACTIVSg10k y ACTIVSg25k (118 a 25 000 barras).
- **Escenarios:** todos los pares de líneas si son ≤ 20 000 (IEEE 118: 15 931); si no, una
  muestra uniforme de 20 000 pares distintos. El orden de caída de cada par es aleatorio.
- **Justicia:** ambas estructuras reciben exactamente los mismos escenarios y consultas;
  cada repetición las reconstruye desde cero y alterna cuál se mide primero; hay un
  calentamiento de 500 escenarios sin registrar; 5 repeticiones; se reporta la mediana.
- **Corrección:** cada respuesta se compara (fuera del cronómetro) con un BFS desde cero.
  Si una sola no coincide, el benchmark se detiene. En la corrida incluida, todas
  coincidieron.
- **Compilación:** `-O2 -DNDEBUG`. El costo de una lectura del cronómetro (≈26 ns) se
  registra en `resultados/configuracion.txt`; las consultas se miden en bloque para
  diluirlo.

### Archivos de salida

| Archivo | Contenido |
|---|---|
| `topologia.csv` | n, m, m/n, puentes, diámetro, grados y escenarios por caso |
| `cortes_n1.csv` | barras que aísla cada puente (lado menor) |
| `resumen.csv` | media, mediana, p95, p99 y máximo por caso, estructura, repetición y operación |
| `escenarios.csv` | tiempos de cada escenario (repetición 1), con el tipo de cada caída |
| `construccion.csv` | tiempo de construcción y memoria estimada |
| `tabla_resumen.md` | tabla lista para pegar en el informe |
| `graficos/` | los 6 gráficos |

### Resultados de la corrida incluida

| Caso | n | Eliminar: D-tree / HKS (µs) | Consultar (µs) | Restaurar (µs) | Memoria (B/barra) |
|---|---|---|---|---|---|
| IEEE 118 | 118 | 0.28 / 1.98 | 0.045 / 0.081 | 0.18 / 0.36 | 110 / 329 |
| IEEE 300 | 300 | 0.33 / 2.06 | 0.049 / 0.084 | 0.15 / 0.47 | 97 / 326 |
| ACTIVSg2000 | 2 000 | 0.48 / 2.92 | 0.062 / 0.124 | 0.24 / 0.56 | 98 / 295 |
| ACTIVSg10k | 10 000 | 0.76 / 3.89 | 0.094 / 0.204 | 0.32 / 0.92 | 85 / 297 |
| ACTIVSg25k | 25 000 | 1.47 / 5.33 | 0.159 / 0.358 | 0.51 / 1.13 | 83 / 291 |

Los tiempos absolutos dependen del equipo; lo comparable entre equipos son las razones y
las tendencias. La interpretación completa está en `PROYECTO.md`.

## Datos

`cargador_matpower` lee las matrices `mpc.bus` y `mpc.branch` del archivo `.m`:

- **Vértices:** barras (`BUS_I`). Se reindexan como 0..n−1 en orden de aparición.
- **Aristas:** ramas en servicio (`F_BUS`, `T_BUS`; se descartan las de `BR_STATUS = 0`).
- Las **líneas paralelas** (varios circuitos entre las mismas barras) se colapsan en una
  sola arista y se descartan los lazos, porque el marco teórico trabaja con grafos simples.

| ACTIVSg2000 | |
|---|---|
| Barras | 2 000 |
| Ramas en el archivo | 3 206 |
| Líneas paralelas colapsadas | 539 |
| Aristas simples | 2 667 |

Las coordenadas del mapa (`ACTIVSg2000_barras.csv`) se extrajeron de la sección `Bus` del
archivo PowerWorld `ACTIVSg2000.aux` del mismo caso, con `herramientas/extraer_coordenadas.py`.

Los casos de `data/casos/` provienen de MATPOWER 8.0 (`github.com/MATPOWER/matpower`,
carpeta `data/`); cada archivo conserva en su cabecera la fuente y la cita que piden sus
autores. ACTIVSg10k y ACTIVSg25k son del mismo repositorio de Texas A&M que ACTIVSg2000.

Fuente (cita requerida por el repositorio): A. B. Birchfield, T. Xu, K. M. Gegner,
K. S. Shetye y T. J. Overbye, "Grid Structural Characteristics as Validation Criteria for
Synthetic Networks", *IEEE Trans. Power Systems* 32(4), 2017.

## Interfaz

Ambas clases (`DTree` y `EulerTourTreeHKS`) exponen la misma interfaz, sobre vértices 0..n−1:

| Método | Descripción |
|---|---|
| `insertar(u, v)` | Inserta la arista. Devuelve `false` si `u == v` o ya existe. |
| `eliminar(u, v)` | Elimina la arista (buscando reemplazo si es del árbol). `false` si no existe. |
| `conectados(u, v)` | Consulta de conectividad. |
| `existe_arista(u, v)` | Indica si la arista está en el grafo. |
| `num_componentes()`, `num_aristas_arbol()`, `num_aristas_fuera()` | Estadísticas. |
| `memoria_bytes()` | Estimación de la memoria de la estructura. |
| `verificar_invariantes()` | Comprueba la consistencia interna (`""` si todo está bien). |

Una contingencia N-2 se expresa como: `eliminar(a)`, `eliminar(b)`, `conectados(...)`,
`insertar(a)`, `insertar(b)`.

## Fidelidad a los papers y decisiones de diseño

### D-tree (Chen, Lachish, Helmer y Böhlen, PVLDB 2022)

Sigue los **Algoritmos 1–8** del paper: reroot, conn, inserción de arista no-árbol
(heurística BFS), inserción de arista del árbol (se re-enraíza el árbol menor), borrado
con búsqueda del reemplazo más cercano a la raíz, link con restauración del centroide,
unlink y borrado de arista no-árbol. La consulta también restaura el centroide.

**Dos discrepancias encontradas con la implementación de referencia en Python**
(`github.com/qingchen3/Imp_bench_dyn`):

1. **Error por uno en la heurística BFS (Algoritmo 3).** El paper sube Δ−2 ancestros
   (`for i = 1 to Δ − 2`); el código de referencia usa `range(1, delta)` con
   `delta = Δ − 2`, que sube solo Δ−3. Aquí se sigue el paper. Ambas versiones mantienen
   la conectividad correcta; difieren en la forma del árbol.

2. **Error lógico en la consulta (Algoritmo 2), presente en el paper y en la
   referencia.** La consulta guarda la raíz de u y luego, al recorrer desde v, puede
   re-enraizar el árbol. Si u y v están en el mismo árbol, la raíz guardada para u queda
   desactualizada y la respuesta es **falsa aunque estén conectados**. Contraejemplo
   mínimo (4 vértices): `insertar(1,2)`, `insertar(3,0)`, `insertar(1,0)`,
   `eliminar(1,2)` y `conn(1,3)` devuelve falso. Aquí la conectividad se decide
   **antes** del segundo re-enraizamiento, que se sigue realizando. Además, como el
   paper indica que la inserción ejecuta primero `conn`, con la versión original una
   respuesta falsa insertaría como arista del árbol una arista dentro del mismo árbol,
   creando un ciclo; la corrección también evita eso.

### Euler Tour Tree HKS (Henzinger y King, J. ACM 1999; Alberts et al., 1997)

- **Representación del paper:** el recorrido de Euler con **2n−1 ocurrencias**
  (`ET(x)`: visitar x; por cada hijo, ET(hijo) y volver a visitar x), guardado en un
  **treap** implícito. Cada vértice tiene una **ocurrencia activa** con sus aristas
  no-árbol, y cada nodo del treap acumula el número de ocurrencias activas y de extremos
  de aristas no-árbol de su subárbol.
- Cada arista del árbol guarda sus dos transiciones en el recorrido (las 4 ocurrencias, o
  3 si un extremo es hoja). Borrar, cambiar la raíz y unir siguen los procedimientos
  1–3 del paper. Cuando quedan dos ocurrencias consecutivas del mismo vértice se elimina
  la inactiva, como en la referencia.
- **Búsqueda de reemplazo HKS (sin niveles):** igual que la referencia, se elige como T1
  la parte con menos extremos de aristas no-árbol; si w(T1) ≥ ⌊log₂² n⌋ se muestrean
  ⌊log₂² n⌋ aristas con probabilidad proporcional a sus extremos en T1, y si ninguna
  reconecta se revisan todas (caso 2 de Henzinger y King). Para n = 2000 son 120 muestras.
- Las aristas no-árbol de cada vértice se guardan en una lista indexada (inserción,
  borrado y acceso aleatorio en O(1) esperado) en lugar del árbol aleatorizado de la
  referencia; cumple el mismo propósito de elegir un vecino al azar.
- El generador aleatorio tiene una semilla fija para que las corridas sean reproducibles.

## Verificación de corrección

`./verificar` compara **cada respuesta** de ambas estructuras contra un oráculo que
recalcula la conectividad por BFS desde cero, y revisa **todas las invariantes internas**:

- D-tree: punteros padre/hijo, ausencia de ciclos, tamaños de subárbol y aristas no-árbol.
- HKS: propiedad de heap y agregados del treap, longitud 2n−1 de cada recorrido, validez
  del recorrido de Euler, transiciones registradas en cada arista y ocurrencias activas.

Casos cubiertos: 300 secuencias aleatorias de 1 500 operaciones sobre grafos de 1 a 40
vértices (comparando todos los pares), familias estructuradas (camino, estrella, ciclo,
completo, grilla, escalera) construidas y destruidas en orden aleatorio, y sobre
ACTIVSg2000 **todas las contingencias N-1** (2 667) y **3 999 contingencias N-2**,
restaurando la red tras cada una. Todo pasa también con AddressSanitizer y
UndefinedBehaviorSanitizer (`make verificar-san`).

Para comprobar que el verificador detecta errores, se inyectaron 9 errores a propósito
(tamaños mal actualizados, reemplazos omitidos, punteros sin redirigir, la consulta
original del paper, entre otros); los 9 fueron detectados.

## Nota sobre la memoria

`memoria_bytes()` suma la capacidad reservada por los arreglos y tablas de cada
estructura. Es una estimación de la huella propia de la estructura, comparable entre
ambas, no una medición del proceso completo.
