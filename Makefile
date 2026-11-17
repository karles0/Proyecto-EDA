# Makefile - Conectividad dinamica: D-tree y Euler Tour Tree HKS
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow
SRC_COMUN = src/dtree.cpp src/ett_hks.cpp src/cargador_matpower.cpp
CABECERAS = src/adyacencia.hpp src/dtree.hpp src/ett_hks.hpp src/cargador_matpower.hpp

all: principal verificar mi_prueba servidor benchmark

principal: src/main.cpp $(SRC_COMUN) $(CABECERAS)
	$(CXX) $(CXXFLAGS) src/main.cpp $(SRC_COMUN) -o $@

verificar: src/verificar.cpp $(SRC_COMUN) $(CABECERAS)
	$(CXX) $(CXXFLAGS) src/verificar.cpp $(SRC_COMUN) -o $@

# Servidor de la interfaz grafica (usa cpp-httplib, incluida en terceros/)
servidor: src/servidor.cpp $(SRC_COMUN) $(CABECERAS) terceros/httplib.h
	$(CXX) $(CXXFLAGS) src/servidor.cpp $(SRC_COMUN) -o $@ -pthread

# Benchmark: siempre sin aserciones para medir solo los algoritmos
benchmark: src/benchmark.cpp $(SRC_COMUN) $(CABECERAS)
	$(CXX) -std=c++17 -O2 -DNDEBUG -Wall -Wextra src/benchmark.cpp $(SRC_COMUN) -o $@

mi_prueba: src/mi_prueba.cpp src/dtree.cpp src/ett_hks.cpp $(CABECERAS)
	$(CXX) $(CXXFLAGS) src/mi_prueba.cpp src/dtree.cpp src/ett_hks.cpp -o $@

# Verificacion con AddressSanitizer y UndefinedBehaviorSanitizer
verificar-san: src/verificar.cpp $(SRC_COMUN) $(CABECERAS)
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer src/verificar.cpp $(SRC_COMUN) -o $@

run: principal
	./principal data/ACTIVSg2000.m

clean:
	rm -f principal verificar mi_prueba servidor benchmark verificar-san data/*_aristas.txt

interfaz: servidor
	./servidor

medir: benchmark
	./benchmark --salida resultados

graficos:
	python3 herramientas/graficar.py resultados

.PHONY: all run interfaz medir graficos clean
