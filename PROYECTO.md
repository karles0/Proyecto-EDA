# Conectividad dinámica en redes eléctricas: D-Tree y Euler Tour Tree HKS

Documento integral del Proyecto de curso I de **CS3014 Estructuras de Datos Avanzadas**
(UTEC, ciclo 2026-2). Reúne todo el trabajo realizado: la exploración de temas, la
elección del problema, el análisis de los datos, la propuesta escrita, la implementación
en C++, la verificación, la interfaz gráfica y los benchmarks con su interpretación.

---

## Índice

1. [Resumen](#1-resumen)
2. [El encargo](#2-el-encargo)
3. [Exploración de temas](#3-exploración-de-temas)
4. [El paper base](#4-el-paper-base)
5. [Conceptos clave](#5-conceptos-clave)
6. [Antecedentes en sistemas de potencia](#6-antecedentes-en-sistemas-de-potencia)
7. [Los datos](#7-los-datos)
8. [Evolución de la pregunta de investigación](#8-evolución-de-la-pregunta-de-investigación)
9. [Las estructuras de datos](#9-las-estructuras-de-datos)
10. [La propuesta escrita](#10-la-propuesta-escrita)
11. [Implementación](#11-implementación)
12. [Verificación de corrección](#12-verificación-de-corrección)
13. [Hallazgos en el paper y en el código de referencia](#13-hallazgos-en-el-paper-y-en-el-código-de-referencia)
14. [Interfaz gráfica](#14-interfaz-gráfica)
15. [Benchmarks](#15-benchmarks)
16. [Cómo reproducir todo](#16-cómo-reproducir-todo)
17. [Pendientes y próximos pasos](#17-pendientes-y-próximos-pasos)
18. [Referencias](#18-referencias)

---

## 1. Resumen

**Problema.** La *conectividad dinámica* consiste en responder, en cualquier momento, si dos
vértices de un grafo están conectados mientras se insertan y eliminan aristas. Una
aplicación representativa es el **análisis de contingencias en redes eléctricas**: se
simula la caída de líneas y se verifica si la red se divide en regiones aisladas
(*islanding*).

**Vacío.** El estudio experimental de referencia (Chen, Böhlen y Helmer, SIGMOD 2025)
motiva todo el problema con redes eléctricas, pero ninguno de sus nueve grafos es una red
eléctrica: usa la red vial de Estados Unidos como sustituto.

**Propuesta.** Comparar el **D-Tree** y el **Euler Tour Tree HKS** (sin niveles) sobre
topologías de redes eléctricas reales, bajo cargas de contingencias **N-2**, en tiempo de
actualización, tiempo de consulta y memoria.

**Resultado principal de los benchmarks.** Sobre cinco redes de 118 a 25 000 barras:

- El **D-Tree gana en el caso típico** en todas las operaciones (borra entre 3.6 y 7 veces
  más rápido, consulta unas 2 veces más rápido) y usa unas **3.5 veces menos memoria**.
- La **detección de islanding es el caso más barato**, no el más caro, porque en estas
  redes la parte que se desprende tiene 1 o 2 barras: el "peor caso" teórico no ocurre.
- Pero la **ventaja del D-Tree se reduce al crecer la red**, porque crece el diámetro
  (de 14 a 91). En las redes de 10 000 y 25 000 barras, **en el peor ~0.3% de los borrados
  el D-Tree es más lento que HKS**: la heurística gana en el caso típico y la estructura
  con garantías gana en el peor caso.

---

## 2. El encargo

| Aspecto | Detalle |
|---|---|
| Curso | CS3014 Estructuras de Datos Avanzadas, UTEC (basado en MIT 6.851) |
| Tarea | Elegir un tema de investigación relacionado con estructuras de datos y plantear una propuesta pequeña, desarrollable en ~6 semanas (semanas 9 a 15) |
| Fuentes válidas | Un paper (o un conjunto pequeño) para estudiar, implementar, extender o comparar; o una propuesta propia bien delimitada |
| Proceso | Primero enviar el tema tentativo con referencias; desarrollar la propuesta solo tras la aprobación |
| Contenido mínimo | Título, motivación, trabajo relacionado, planteamiento, plan de trabajo y entregable esperado |
| Extensión | 2–4 páginas según el enunciado; luego el profesor indicó **6–7 páginas como máximo, con a lo más 2 de marco teórico**, y compartió una propuesta de tesis como modelo de estructura |
| Modalidad y plazo | Hasta 3 personas; entrega el viernes 25 de septiembre de 2026 |

**Temario del curso** (sección 6 del sílabo): temporales (máquina de puntero, persistencia
parcial/total/confluente/funcional, retroactividad), geométricas (point location, range
trees, fractional cascading), optimalidad dinámica (BST, splay trees, vista geométrica),
jerarquía de memoria (B-trees, colas de prioridad y range searching cache-oblivious),
enteros (predecesor, van Emde Boas, x/y-fast tries), árboles estáticos (LCA, RMQ, suffix
trees/arrays), estructuras sucintas (rank/select, arreglos de sufijos compactos) y
**grafos dinámicos (link-cut trees, heavy-light decomposition, Euler tour trees)**.

---

## 3. Exploración de temas

Se evaluaron candidatos de casi todas las áreas del temario, con un criterio central: que
el **uso de la estructura estuviera justificado por una aplicación real** y que hubiera
**datos públicos** para experimentar.

| Área | Idea evaluada | Por qué no se eligió |
|---|---|---|
| Sucintas (§6.8) | *Move structure* y r-index para pangenomas (Movi) | Muy atractiva, pero de implementación costosa para 6 semanas |
| Enteros (§6.6) | Índices aprendidos (PGM-index) frente a estructuras de predecesor, con el benchmark SOSD | Datos fáciles, pero tema muy trabajado |
| Memoria (§6.5) | B-trees cache-oblivious y B^ε-trees (BetrFS) | Justificación excelente, pero menos cercano al interés del grupo |
| Optimalidad dinámica (§6.4) | Greedy, tango trees, árboles con predicciones | Justificación aplicada débil |
| Retroactividad (§6.2.3) | Problemas de grafos retroactivos (Henzinger y Wu) | Muy teórico |
| Persistencia (§6.2.2) | B-trees copy-on-write y diccionarios versionados (Rodeh; Twigg et al.) | Finalista |
| Geometría (§6.3) | Range trees frente a índices multidimensionales aprendidos (Flood, Tsunami; datos de taxis de Nueva York) | Interesante, pero se priorizó grafos |
| Persistencia aplicada | Merkle Patricia Trie de Ethereum y Verkle trees | Finalista; exige criptografía (compromisos vectoriales) |
| Grafos dinámicos (§6.9) | **Conectividad dinámica (Chen et al. 2025)** | **Elegido** |
| Grafos dinámicos | Conectividad dinámica sobre el grafo de transacciones de Bitcoin | Finalista; sin ningún antecedente que una ambos temas |

**Comparación final de los cuatro finalistas:**

| | Conectividad dinámica | CoW B-trees | MPT/Verkle | Bitcoin + conectividad |
|---|---|---|---|---|
| Dificultad | Baja–media | Media | Alta | Media |
| Datos | Triviales | Medios | Medios–altos | Medios |
| Antecedentes con implementación | **Excelentes** | Muy buenos | Muy buenos | Ninguno |
| Riesgo | Bajo | Bajo–medio | Alto | Medio–alto |

Se eligió **conectividad dinámica**, y luego se enfocó en **redes eléctricas** a partir de
una mención del propio paper base.

> **Lección de verificación de fuentes.** Al revisar la opción de B-trees copy-on-write se
> detectó que el paper de Soules et al. (FAST 2003) nunca usa el término "CoW B-tree": esa
> etiqueta es de Twigg et al. al citarlo, y la comparación de 152 GB contra 4 GB mezclaba
> metadatos de archivos con directorios. La fuente que trata explícitamente el CoW B-tree
> es Rodeh (ACM TOS, 2008). Desde entonces, cada afirmación se contrastó con el texto
> original antes de usarla.

---

## 4. El paper base

**Q. Chen, M. H. Böhlen y S. Helmer, "An Experimental Comparison of Tree-data Structures
for Connectivity Queries on Fully-dynamic Undirected Graphs", *Proc. ACM Manag. Data*
3(1), SIGMOD, art. 10, 2025.**

**Qué hicieron.** Implementaron en Python y compararon las **diez** estructuras
principales para conectividad dinámica, varias nunca implementadas antes:

- Sin niveles: **D-Tree** y **link-cut tree (LCT)**.
- Basadas en **Euler tour trees**: HK (Henzinger–King), **HKS** (HK simplificado, sin
  niveles) y HDT (Holm–de Lichtenberg–Thorup).
- De altura acotada: structural tree (ST, STV), local tree (LT, LTV) y lazy local tree (LzT).

Publicaron el código en `github.com/qingchen3/Imp_bench_dyn`, con los sellos de ACM
*Artifacts Available* y *Artifacts Evaluated – Reusable*.

**Sus nueve grafos.** Cada dataset es un grafo; los eligieron para cubrir **formas**
distintas, no dominios:

| Grafo | Tipo | Diámetro | Para qué sirve |
|---|---|---|---|
| Estrella, camino, completo, aleatorio, power-law | Sintéticos (NetworkX) | El camino es grande | Forzar casos extremos y aislar causas |
| YouTube, Stack Overflow | Redes sociales reales | Pequeño | El caso típico de grafos reales |
| Red vial de EE.UU. | Real | **Grande** | El caso real de diámetro grande |
| Semantic Scholar | Real, 65 M vértices | Pequeño | Escalabilidad extrema |

**Resultados que citamos:** ninguna estructura es robusta en la práctica; HK usa hasta 85
veces más memoria que el D-Tree; HDT llega a ser 1335 veces más lento que HKS en el
grafo camino; borrar en LCT llega a ser 10⁴ veces más lento que en las demás; el D-Tree
domina en grafos de diámetro pequeño pero **se degrada en los de diámetro grande**; las
estructuras con mejores costos amortizados resultaron las más lentas. Los autores piden
explícitamente un benchmark y cargas de trabajo estandarizados.

**El vacío.** En su introducción motivan el problema con redes eléctricas e *islanding*,
pero ninguno de sus grafos es una red eléctrica: la red vial entra como sustituto porque
también tiene diámetro grande. Además, sus cargas eliminan aristas al azar
(parámetro u_r = inserciones/eliminaciones), lo que no representa el patrón
*borrar–consultar–restaurar* del análisis de contingencias.

---

## 5. Conceptos clave

- **Grafo dinámico.** Recibe inserciones y eliminaciones de aristas intercaladas con consultas.
- **Árbol de expansión.** Árbol que conecta todos los vértices de una componente con
  n − 1 aristas. Las m − (n − 1) aristas restantes quedan **fuera del árbol**.
- **Arista de reemplazo.** Al borrar una arista del árbol, este se parte en dos; una arista
  de reemplazo es una arista fuera del árbol que vuelve a unir las partes. Buscarla es la
  operación cara de todas las estructuras.
- **Puente.** Arista cuya eliminación desconecta su componente; equivalentemente, la que no
  pertenece a ningún ciclo. En una red eléctrica es una **línea crítica**.
- **Contingencia N-k.** Escenario en el que k líneas salen de servicio a la vez.
  **Islanding:** la división de la red en regiones aisladas.
- **Densidad m/n.** Un árbol tiene m/n ≈ 1. Las redes eléctricas son **casi árboles**.

**Por qué N-1 no basta y se trabaja con N-2.** Las contingencias N-1 que desconectan la
red son exactamente los puentes, y todos se obtienen con una sola pasada del algoritmo de
Tarjan en O(n + m), sin necesidad de una estructura dinámica. En N-2, borrar una arista
puede convertir en puente a otra que no lo era (por ejemplo, al romper un ciclo), así que
el segundo borrado depende del estado que dejó el primero y el precálculo deja de bastar.

**"Que la búsqueda falle" no es un error.** Cuando no aparece arista de reemplazo,
significa que **sí hubo islanding**: es el resultado que el ingeniero quiere detectar.
En teoría ese es el caso más caro, porque para afirmar "no hay reemplazo" hay que revisar
toda la parte menor.

---

## 6. Antecedentes en sistemas de potencia

No existe un trabajo que aplique estas estructuras a redes eléctricas; sí existen piezas
de ambos lados:

| Trabajo | Qué aporta | Qué le falta |
|---|---|---|
| Goderya, Metwally y Mansour (1980) | Detección e identificación rápida de islas en redes de potencia | Es el antecedente clásico; no usa estructuras dinámicas |
| Mahat, Chen y Bak-Jensen (2008) | Revisión de métodos de detección de islanding | Enfoque de generación distribuida |
| *Graph Computing based Fast Screening in Contingency Analysis* (arXiv:1904.00044) | Screening N-1 con detección de puentes sobre IEEE 118 y un caso real | Estático: no escala a N-k |
| *HPC enabled contingency analysis* (arXiv:2512.08465) | N-2 recorriendo el grafo resultante (tipo DFS) | Recomputa desde cero y escala paralelizando |
| *DRL for cascading failure mitigation* (arXiv:2505.09012) | Usa union-find para detectar islas | Union-find no admite borrados: reconstruye en cada escenario |
| *Algebraic-graph method for islanding* (EPSR, 2011) | Declara que los métodos evalúan cada contingencia por separado | Propone un método espectral |

---

## 7. Los datos

### 7.1 Fuente

**Electric Grid Test Case Repository de Texas A&M**: redes **sintéticas** diseñadas para ser
estadística y funcionalmente realistas sin exponer infraestructura crítica. Se usa
**ACTIVSg2000** (2000 barras, Texas). Entre sus variantes (ACTIVSg2000, Texas2k Series24 y
Series25) se eligió la canónica, porque las otras comparten la topología y solo cambian
condiciones de operación, que no afectan la conectividad.

Para el benchmark se agregaron casos de distintos tamaños, tomados del repositorio oficial
de MATPOWER 8.0: **IEEE 118**, **IEEE 300**, **ACTIVSg10k** y **ACTIVSg25k**.

### 7.2 Conversión a grafo

Del formato MATPOWER se leen las barras (`mpc.bus`) como vértices y las ramas en servicio
(`mpc.branch`) como aristas. Las **líneas paralelas** (varios circuitos entre las mismas
barras) se colapsan en una sola arista, porque el marco teórico trabaja con grafos simples.

### 7.3 Caracterización de ACTIVSg2000

| Métrica | Valor |
|---|---|
| Barras (n) | 2 000 |
| Ramas en el archivo | 3 206 |
| Líneas paralelas colapsadas | 539 |
| Aristas simples (m) | 2 667 |
| m/n | **1.33** |
| Aristas del árbol / fuera del árbol | 1 999 / 668 (25%) |
| Puentes | 457 (17.1%) |
| Grado medio / máximo | 2.67 / 16 |
| Barras de grado 1 | 456 |
| Diámetro | 26 |

**El hallazgo que reorientó el proyecto.** Al cortar cada puente (N-1), la parte que se
desprende tiene **mediana 1 y máximo 2 barras**: todos los puentes son ramales terminales.
En N-2, 354 pares de líneas desprenden 3 o más barras, con **mediana 3 y máximo 11** de
2 000. Al borrar una línea aparecen en promedio 0.9 puentes nuevos (mediana 0, máximo 7).
El caso que en teoría es caro —recorrer una parte grande sin encontrar reemplazo— **no
ocurre** en esta red.

El caso también incluye una tabla oficial de contingencias (`contab_ACTIVSg2000.m`) con
3 734 escenarios, que queda como opción para trabajos futuros.

### 7.4 Los cinco casos del benchmark

| Caso | n | m | m/n | Puentes | Diámetro |
|---|---|---|---|---|---|
| IEEE 118 | 118 | 179 | 1.52 | 9 (5%) | 14 |
| IEEE 300 | 300 | 409 | 1.36 | 90 (22%) | 24 |
| ACTIVSg2000 | 2 000 | 2 667 | 1.33 | 457 (17%) | 26 |
| ACTIVSg10k | 10 000 | 12 217 | 1.22 | 3 980 (33%) | 51 |
| ACTIVSg25k | 25 000 | 30 110 | 1.20 | 10 940 (36%) | 91 |

Todas son casi árboles, y al crecer aumentan el diámetro y la proporción de puentes.

---

## 8. Evolución de la pregunta de investigación

1. **Primera idea: link-cut tree contra Euler tour tree.** Se descartó por tres razones:
   el LCT no trae la búsqueda de arista de reemplazo (sus `path_pointer` dirigidos impiden
   recorrer el árbol hacia abajo para enumerar una componente, y los autores tuvieron que
   extenderlo); su fortaleza real son las consultas y agregados sobre **caminos** (máximo,
   mínimo, suma), que la detección de islanding no usa; y el paper ya mostró que pierde
   por 10⁴. La comparación tendría un resultado conocido de antemano.
2. **Elección: D-Tree contra Euler Tour Tree HKS.** Ambos están diseñados para este
   problema y representan la tensión central del paper: una **heurística liviana** contra
   una estructura **balanceada con garantías**.
3. **Hipótesis tras analizar los datos:** como las redes eléctricas son casi árboles y las
   partes que se desprenden son diminutas, la maquinaria del Euler Tour Tree es sobrecarga
   que no se amortiza, y debería ganar el D-Tree. La **tensión**: el D-Tree se degrada en
   grafos de diámetro grande, y las redes eléctricas lo son.

**Pregunta final:** *¿cómo se comparan el D-Tree y el Euler Tour Tree HKS en tiempo de
actualización, tiempo de consulta y uso de memoria cuando el grafo es una red eléctrica y
la carga de trabajo corresponde a un análisis de contingencias?*

---

## 9. Las estructuras de datos

### 9.1 D-Tree (Chen, Lachish, Helmer y Böhlen, PVLDB 2022)

Mantiene un árbol de expansión por componente **sin niveles**. `conn(u, v)` sube desde u y
v hasta sus raíces, así que su costo es la suma de sus profundidades. El costo promedio es
óptimo cuando el árbol minimiza la **suma de distancias** S_d, y el árbol que lo logra es
un árbol BFS cuya raíz es un **centroide**: un nodo cuya eliminación deja subárboles de a
lo más n/2 nodos.

Con la raíz en un centroide, al borrar la arista de un nodo x con su padre, la parte
desprendida es siempre el subárbol de x y siempre la menor; el costo de buscar reemplazo
(el **número de corte**) es el tamaño de ese subárbol, y la suma de los números de corte
coincide con S_d. Un S_d bajo abarata a la vez consultas y búsquedas de reemplazo.

Mantener el óptimo es caro, así que usa **heurísticas**: re-enraizar el árbol menor al
unir, preferir el reemplazo más cercano a la raíz, y restaurar el centroide durante los
recorridos hacia la raíz. Cada nodo guarda clave, padre, hijos, tamaño del subárbol y
vecinos fuera del árbol, por lo que **ocupa poca memoria**. Su debilidad: **sin garantías en
el peor caso**, y en grafos de diámetro grande los caminos largos degradan el rendimiento.

### 9.2 Euler Tour Tree (Henzinger y King, J. ACM 1999)

Representa cada árbol por su **recorrido de Euler**, una secuencia de **2n − 1
ocurrencias** guardada en un árbol binario balanceado cuyo inorden la reproduce. Las
modificaciones se vuelven operaciones sobre la secuencia: borrar una arista es extraer un
intervalo, cambiar la raíz es rotar la secuencia y unir dos árboles es insertar una
secuencia en otra, todo en O(log n). Cada vértice tiene una **ocurrencia activa** con sus
aristas fuera del árbol, y cada nodo cuenta cuántas hay en su subárbol, lo que permite
identificar la parte menor sin recorrerla. La versión original reparte las aristas en
O(log n) **niveles**, de donde sale su cota polilogarítmica amortizada; el precio es la
memoria.

**HKS** (Alberts, Cattaneo e Italiano, 1997) es la variante **sin niveles**: un solo bosque,
muestreo aleatorio de aristas fuera del árbol incidentes a la parte menor y, si ninguna
reconecta, revisión de todas. Chen et al. la eligen sobre HK y HDT por rendimiento
comparable con menor complejidad.

Se verificó programáticamente que el ejemplo de la figura de la propuesta (recorrido
A₁ B₁ A₂ C₁ D₁ C₂ E₁ C₃ A₃, árbol balanceado y borrado de (A, C)) es correcto y corresponde
a HKS: la representación con recorrido de Euler es la misma en HK y HKS, y HKS mantiene un
único árbol por componente, que es justo lo que muestra la figura.

---

## 10. La propuesta escrita

**Proceso.** Un primer borrador en LaTeX sirvió para definir la estructura, que luego se
pasó al documento Typst del grupo. El marco teórico pasó por varias iteraciones:
primero demasiado breve, luego demasiado extenso, hasta llegar a la versión final.

**Estructura final (Typst):**

1. **Introducción.** El problema y su aplicación; el panorama de estructuras; el estudio de
   Chen et al. y los tipos de grafos que usaron; las redes eléctricas y el vacío; lo que se
   busca establecer.
2. **Marco teórico.** Preliminares (grafo; camino, ciclo y componente conexa; diámetro;
   árbol de expansión; puente; contingencia N-k e islanding; arista de reemplazo;
   estructuras de conectividad dinámica), D-Tree y Euler Tour Tree con HKS, con una figura
   para cada uno.
3. **Trabajo relacionado.** El paper base y el vacío que deja.
4. **Planteamiento.** Qué se implementa, sobre qué datos y con qué carga, y la pregunta de
   investigación.
5. **Plan de trabajo.** Cinco actividades en seis semanas: revisión, diseño,
   implementación, experimentación, y análisis y redacción.
6. **Entregable esperado.** Implementaciones, benchmarks e informe.
7. **Referencias** en formato IEEE.

**Decisiones de redacción:**

- Marco teórico de unas dos páginas, con solo las definiciones que el proyecto usa y
  explicaciones en prosa (al estilo de la propuesta modelo), sin narrar mejoras de
  complejidad ni hablar de lo que "evaluaremos": eso pertenece al Planteamiento.
- Figura del D-Tree adaptada del paper original (dos D-Trees con los atributos de cada nodo)
  y figura del Euler Tour Tree con un ejemplo propio verificado; la explicación del ejemplo
  va en el pie de cada figura.
- Las **cinco referencias indispensables**: el paper base, el del D-Tree, el de Henzinger y
  King, el de HKS (Alberts et al.) y Goderya et al. para islanding; se eliminaron las
  redundantes.
- Correcciones: HKS significa "HK simplificado" (no "Heinzinger-King-Serna"); el apellido es
  Böhlen; el Euler Tour Tree se presentó en STOC 1995 y la versión de revista es de 1999,
  así que se cita sin año en el texto.

**Título sugerido:** *Cuando cae una línea: conectividad dinámica en redes eléctricas con
D-Tree y HKS*.

---

## 11. Implementación

Todo está en C++17, sin dependencias salvo cpp-httplib (solo para la interfaz).

| Archivo | Contenido |
|---|---|
| `src/dtree.hpp/.cpp` | D-Tree: Algoritmos 1–8 del paper |
| `src/ett_hks.hpp/.cpp` | Euler Tour Tree HKS: recorrido de 2n − 1 ocurrencias en un treap implícito |
| `src/adyacencia.hpp` | Conjunto de aristas fuera del árbol con inserción, borrado y acceso aleatorio en O(1) |
| `src/cargador_matpower.hpp/.cpp` | Lectura de MATPOWER y conversión a grafo simple |
| `src/main.cpp` | Carga una red, construye ambas estructuras y corre una contingencia de ejemplo |
| `src/mi_prueba.cpp` | Caso de 5 vértices verificable a mano |
| `src/verificar.cpp` | Verificación de corrección |
| `src/servidor.cpp` + `web/index.html` | Interfaz gráfica |
| `src/benchmark.cpp` + `herramientas/graficar.py` | Benchmark y gráficos |

**Interfaz común** de ambas estructuras: `insertar`, `eliminar`, `conectados`,
`existe_arista`, `es_arista_arbol`, `representante`, `num_componentes`,
`num_aristas_arbol`, `num_aristas_fuera`, `memoria_bytes` y `verificar_invariantes`.

**Decisiones de diseño del HKS** (alineadas con la implementación de referencia): la parte
menor T1 se elige por número de aristas fuera del árbol; se muestrean ⌊log₂² n⌋ aristas
(120 para n = 2000) si w(T1) lo permite; las aristas fuera del árbol se guardan en listas
indexadas en lugar de árboles aleatorizados, con el mismo propósito; cuando quedan dos
ocurrencias consecutivas del mismo vértice se elimina la inactiva. El generador aleatorio
tiene semilla fija para que las corridas sean reproducibles.

---

## 12. Verificación de corrección

`./verificar` compara **cada respuesta** de ambas estructuras con un oráculo que recalcula
la conectividad por BFS desde cero, y revisa **todas las invariantes internas**:

- **D-Tree:** punteros padre/hijo, ausencia de ciclos, tamaños de subárbol y aristas fuera
  del árbol.
- **HKS:** propiedad de heap y agregados del treap, longitud 2n − 1 de cada recorrido,
  validez del recorrido de Euler, transiciones registradas en cada arista y ocurrencias
  activas.

**Cobertura:** 300 secuencias aleatorias de 1 500 operaciones sobre grafos de 1 a 40
vértices (comparando todos los pares); familias estructuradas (camino, estrella, ciclo,
completo, grilla, escalera) construidas y destruidas en orden aleatorio; y sobre
ACTIVSg2000 **todas las contingencias N-1** (2 667) más 3 999 N-2. Todo pasa también con
AddressSanitizer y UndefinedBehaviorSanitizer.

**Pruebas de mutación:** se inyectaron 9 errores a propósito (tamaños mal actualizados,
reemplazos omitidos, punteros sin redirigir, orden de transiciones invertido, pesos sin
propagar, la consulta original del paper, entre otros). **Los 9 fueron detectados**, lo que
confirma que el verificador no aprueba código incorrecto.

---

## 13. Hallazgos en el paper y en el código de referencia

1. **Error por uno en el código de referencia (D-Tree, Algoritmo 3).** El paper sube Δ − 2
   ancestros (`for i = 1 to Δ − 2`); el código Python usa `range(1, delta)` con
   `delta = Δ − 2`, que sube solo Δ − 3. Aquí se sigue el paper. Ambas versiones mantienen
   la conectividad correcta; difieren en la forma del árbol.
2. **Error lógico en la consulta del paper (Algoritmo 2), también en la referencia.** La
   consulta guarda la raíz de u y luego, al recorrer desde v, puede re-enraizar el árbol;
   si u y v están en el mismo árbol, la raíz guardada queda desactualizada y la respuesta
   es falsa aunque estén conectados.
   **Contraejemplo mínimo (4 vértices, 5 operaciones):** `insertar(1,2)`, `insertar(3,0)`,
   `insertar(1,0)`, `eliminar(1,2)`, y `conn(1,3)` devuelve falso aunque 1–0–3 están
   conectados. Además, como el paper indica que la inserción ejecuta primero `conn`, una
   respuesta falsa lleva a insertar como arista del árbol una arista dentro del mismo árbol,
   **creando un ciclo**. La corrección decide la conectividad antes del segundo
   re-enraizamiento.

Ambos hallazgos merecen una mención en el informe final.

---

## 14. Interfaz gráfica

```
Navegador (web/index.html)  ──HTTP/JSON──►  ./servidor (C++)  ──►  DTree + EulerTourTreeHKS
       mapa, clics, panel        ◄──────────   respuestas          + verificación por BFS
```

La interfaz **usa las implementaciones en C++**; cada acción se ejecuta sobre ambas
estructuras y se contrasta con un BFS.

- **Mapa geográfico real:** las coordenadas de las 2 000 barras (1 250 subestaciones de
  Texas) se extrajeron del archivo PowerWorld `.aux` del dataset. Las líneas se colorean
  por tensión (500, 230, 161 y 115 kV, más transformadores), como en los mapas de las
  empresas eléctricas; el magenta se reserva para alarmas (líneas caídas y zonas aisladas).
- **Hacer caer líneas:** clic en una línea para hacerla caer y otro para restaurarla. Para
  cada estructura se indica si la línea estaba en su árbol de expansión, si encontró
  reemplazo o hubo islanding, y el tiempo de la operación.
- **Consultar conectividad** entre dos barras.
- **Escenarios:** caída aleatoria, caída de un puente, el ejemplo N-2 con islanding
  (6173–6265 y 6092–6340, que aísla 11 barras) y restaurar la red.
- **Resaltar puentes** (algoritmo de Tarjan) y **acercar a la zona aislada**.

**Observación útil para la presentación.** En el ejemplo N-2, la primera línea que cae está
en el árbol del D-Tree, que tiene que buscar un reemplazo, pero fuera del árbol de HKS, que
no hace nada: **cada estructura construye su propio árbol de expansión**, aunque ambas
respondan lo mismo.

---

## 15. Benchmarks

### 15.1 Carga de trabajo y metodología

Cada **escenario** es una contingencia N-2: `eliminar(e1)`, `eliminar(e2)`, 10 consultas
(los extremos de las dos líneas, que detectan el islanding, más 8 pares al azar),
`insertar(e2)` e `insertar(e1)`.

| Aspecto | Decisión | Por qué |
|---|---|---|
| Casos | 5 redes, de 118 a 25 000 barras | La "escalera" de la propuesta; permite ver tendencias |
| Escenarios | Todos los pares si son ≤ 20 000 (IEEE 118: 15 931); si no, 20 000 al azar | N-2 completo en 25k serían ~453 millones de pares |
| Misma carga | Ambas estructuras reciben los mismos escenarios y consultas | Comparación justa |
| Repeticiones | 5, reconstruyendo las estructuras y alternando cuál va primero | Estabilidad y sin sesgo de orden ni de caché |
| Calentamiento | 500 escenarios sin registrar | Evita medir el arranque en frío |
| Corrección | Cada respuesta se compara con BFS fuera del cronómetro | Un tiempo rápido con una respuesta errónea no vale |
| Compilación | `-O2 -DNDEBUG` | Se mide solo el algoritmo |
| Cronómetro | Costo medido (~26 ns) y registrado; consultas medidas en bloque | Rigor en operaciones de decenas de nanosegundos |
| Estadísticos | Mediana de 5 repeticiones; media, mediana, p95, p99 | El máximo individual no es confiable (interrupciones del sistema) |

En la corrida incluida (≈96 000 contingencias por estructura y repetición) **todas las
respuestas coincidieron con el BFS**. La variación entre repeticiones (máximo ÷ mínimo de
las medias) fue de 2 a 17% en borrados y restauraciones; llegó a 51% solo en las consultas
de las redes más chicas, que duran unos 50 nanosegundos.

### 15.2 Resultados

**Tiempo medio por operación (µs) y memoria:**

| Caso | n | Eliminar D-Tree / HKS | HKS ÷ D-Tree | Consultar D-Tree / HKS | Restaurar D-Tree / HKS | Memoria (B/barra) |
|---|---|---|---|---|---|---|
| IEEE 118 | 118 | 0.28 / 1.98 | 6.95 | 0.045 / 0.081 | 0.18 / 0.36 | 110 / 329 |
| IEEE 300 | 300 | 0.33 / 2.06 | 6.22 | 0.049 / 0.084 | 0.15 / 0.47 | 97 / 326 |
| ACTIVSg2000 | 2 000 | 0.48 / 2.92 | 6.08 | 0.062 / 0.124 | 0.24 / 0.56 | 98 / 295 |
| ACTIVSg10k | 10 000 | 0.76 / 3.89 | 5.13 | 0.094 / 0.204 | 0.32 / 0.92 | 85 / 297 |
| ACTIVSg25k | 25 000 | 1.47 / 5.33 | 3.62 | 0.159 / 0.358 | 0.51 / 1.13 | 83 / 291 |

**Eliminar según el resultado de la búsqueda (mediana, µs; repetición 1):**

| Caso | Fuera del árbol D-Tree / HKS | Con reemplazo D-Tree / HKS | Islanding D-Tree / HKS |
|---|---|---|---|
| IEEE 118 | 0.13 / 0.26 | 0.26 / 2.44 | 0.10 / 0.92 |
| ACTIVSg2000 | 0.14 / 0.37 | 0.39 / 3.74 | 0.10 / 1.32 |
| ACTIVSg25k | 0.29 / 1.04 | 0.89 / 6.93 | 0.21 / 2.41 |

**Cola extrema al eliminar (percentil 99.9, µs):**

| Caso | D-Tree | HKS |
|---|---|---|
| IEEE 118 | 2.2 | 22.5 |
| ACTIVSg2000 | 22.7 | 28.7 |
| ACTIVSg10k | **95.0** | 38.1 |
| ACTIVSg25k | **191.0** | 65.4 |

### 15.3 Gráficos

| Gráfico | Qué muestra | Parte del proyecto a la que responde |
|---|---|---|
| `1_topologia.png` | m/n, % de puentes, diámetro y barras que aísla cada puente | Sustenta la hipótesis: casi árboles con cortes diminutos |
| `2_tiempo_por_operacion.png` | Tiempo medio de eliminar, consultar y restaurar por caso | Responde la pregunta de investigación (tiempos) |
| `3_escalabilidad.png` | Crecimiento con n y razón HKS ÷ D-Tree | La tensión con el diámetro |
| `4_eliminacion_por_tipo.png` | Eliminar según fuera del árbol, con reemplazo o islanding | Conecta con el marco teórico (arista de reemplazo) |
| `5_distribucion_eliminacion.png` | Cola de la distribución de tiempos | Heurística contra garantías en el peor caso |
| `6_memoria_y_construccion.png` | Bytes por barra y tiempo de construcción | Responde la pregunta de investigación (memoria) |

### 15.4 Interpretación

1. **El D-Tree domina en el caso típico.** Gana en las tres operaciones y en los cinco
   casos, y usa unas 3.5 veces menos memoria. Esto confirma la hipótesis de que la
   maquinaria del Euler Tour Tree no se amortiza en estas redes.
2. **El "peor caso" teórico es el más barato.** Detectar islanding (búsqueda sin reemplazo)
   cuesta ~0.1–0.2 µs en el D-Tree, porque la parte desprendida tiene 1 o 2 barras
   (gráfico 1). Es la evidencia más directa de la hipótesis.
3. **El caso caro es "con reemplazo".** Es el único donde se recorre una parte no trivial, y
   ahí se concentran las diferencias de rendimiento.
4. **La ventaja del D-Tree se reduce al crecer la red.** La razón HKS ÷ D-Tree al eliminar
   baja de 6.95 a 3.62 mientras el diámetro crece de 14 a 91, coherente con la degradación
   en diámetro grande que reporta el paper.
5. **Las colas se cruzan en las redes grandes.** En ACTIVSg10k y 25k, a partir del
   percentil ~99.7 el D-Tree es más lento que HKS (p99.9: 95 contra 38 µs y 191 contra
   65 µs). La heurística gana casi siempre, pero la estructura con garantías acota mejor el
   peor caso.

**Respuesta a la pregunta de investigación.** En redes eléctricas bajo contingencias N-2,
el D-Tree es más rápido en actualización y consulta, y más liviano en memoria, en el caso
típico. Sin embargo, ninguna estructura domina en todas las métricas: en redes grandes,
de diámetro grande, el D-Tree pierde en la cola extrema de los borrados. Esto coincide con
lo anticipado en la sección de resultados esperados de la propuesta.

### 15.5 Limitaciones

- Los tiempos absolutos dependen del equipo y del compilador; lo comparable entre equipos
  son las razones y las tendencias.
- No son comparables en valor absoluto con los del paper base, que usa Python; sí lo son
  las tendencias cualitativas.
- `memoria_bytes()` es una estimación de la huella de cada estructura, consistente entre
  ambas, no una medición del proceso completo.
- Las contingencias N-2 se muestrean uniformemente; la tabla oficial de contingencias del
  caso queda como trabajo futuro.
- Las redes son sintéticas (realistas por diseño, pero no reales).

---

## 16. Cómo reproducir todo

En Ubuntu, dentro de la carpeta del proyecto:

```bash
sudo apt install build-essential unzip python3-venv   # solo la primera vez
make                                                  # compila todo
./mi_prueba                                           # caso pequeño verificable a mano
./principal                                           # carga ACTIVSg2000 y corre un ejemplo
./verificar                                           # verificación de corrección
./servidor                                            # interfaz en http://localhost:8080
./benchmark                                           # benchmark (~1 minuto) → resultados/
python3 -m venv .venv && source .venv/bin/activate
pip install -r herramientas/requirements.txt
python3 herramientas/graficar.py                      # gráficos → resultados/graficos/
deactivate
```

Si el puerto 8080 está ocupado: `./servidor --puerto 8081`.

---

## 17. Pendientes y próximos pasos

- **Orden de las citas IEEE en la introducción.** En el tercer párrafo la cita [2] (D-Tree)
  aparece antes que [1]; conviene devolver el [1] al final del segundo párrafo, cuya
  afirmación proviene del paper base.
- **Mencionar en el informe los dos hallazgos** de la sección 13.
- **Correr el benchmark en sus equipos** y reportar las razones, no solo los tiempos
  absolutos.
- **Extensiones posibles:** usar la tabla oficial de contingencias de ACTIVSg2000, probar
  contingencias N-3, incorporar ACTIVSg70k, o comparar con la red vial de EE.UU. para
  contrastar directamente con el sustituto que usa el paper.

---

## 18. Referencias

[1] Q. Chen, M. H. Böhlen y S. Helmer, "An experimental comparison of tree-data structures
for connectivity queries on fully-dynamic undirected graphs," *Proc. ACM Manag. Data*,
vol. 3, no. 1, art. 10, 2025, doi: 10.1145/3709660.

[2] Q. Chen, O. Lachish, S. Helmer y M. H. Böhlen, "Dynamic spanning trees for
connectivity queries on fully-dynamic undirected graphs," *Proc. VLDB Endow.*, vol. 15,
no. 11, pp. 3263–3276, 2022, doi: 10.14778/3551793.3551868.

[3] F. Goderya, A. A. Metwally y O. Mansour, "Fast detection and identification of islands
in power networks," *IEEE Trans. Power App. Syst.*, vol. PAS-99, no. 1, pp. 217–221, 1980,
doi: 10.1109/TPAS.1980.319631.

[4] M. R. Henzinger y V. King, "Randomized fully dynamic graph algorithms with
polylogarithmic time per operation," *J. ACM*, vol. 46, no. 4, pp. 502–516, 1999,
doi: 10.1145/320211.320215.

[5] D. Alberts, G. Cattaneo y G. F. Italiano, "An empirical study of dynamic graph
algorithms," *ACM J. Exp. Algorithmics*, vol. 2, art. 5, 1997, doi: 10.1145/264216.264223.

**Datos:** A. B. Birchfield, T. Xu, K. M. Gegner, K. S. Shetye y T. J. Overbye, "Grid
structural characteristics as validation criteria for synthetic networks," *IEEE Trans.
Power Syst.*, vol. 32, no. 4, pp. 3258–3265, 2017. Casos IEEE 118 e IEEE 300 vía MATPOWER 8.0
(R. D. Zimmerman, C. E. Murillo-Sánchez y R. J. Thomas).

**Otras fuentes consultadas:** Holm, de Lichtenberg y Thorup (J. ACM, 2001); Sleator y
Tarjan (link-cut trees, 1983); Tarjan y Vishkin (1985); Seidel y Aragon (1996); Mahat,
Chen y Bak-Jensen (2008); arXiv:1904.00044; arXiv:2512.08465; arXiv:2505.09012.
