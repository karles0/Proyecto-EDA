
#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct GrafoRed {
    int n = 0;
    std::vector<long long> barra_id;
    std::unordered_map<long long, int> indice;
    std::vector<std::pair<int, int>> aristas;

    int ramas_totales = 0;
    int ramas_fuera_servicio = 0;
    int lazos = 0;
    int paralelas = 0;
};

GrafoRed cargar_matpower(const std::string& ruta);

void escribir_lista_aristas(const GrafoRed& g, const std::string& ruta);
