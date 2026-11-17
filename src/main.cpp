
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "cargador_matpower.hpp"
#include "dtree.hpp"
#include "ett_hks.hpp"

namespace {

std::string ruta_lista_aristas(const std::string& ruta_caso) {
    const std::size_t p = ruta_caso.rfind(".m");
    const std::string base = (p != std::string::npos && p + 2 == ruta_caso.size()) ? ruta_caso.substr(0, p) : ruta_caso;
    return base + "_aristas.txt";
}

double kib(std::size_t bytes) { return static_cast<double>(bytes) / 1024.0; }

std::pair<int, int> linea(const GrafoRed& g, long long a, long long b) {
    const auto ia = g.indice.find(a), ib = g.indice.find(b);
    if (ia == g.indice.end() || ib == g.indice.end()) return {-1, -1};
    return {ia->second, ib->second};
}

}

int main(int argc, char** argv) {
    const std::string ruta = argc > 1 ? argv[1] : "data/ACTIVSg2000.m";
    try {

        const GrafoRed g = cargar_matpower(ruta);
        const std::string ruta_aristas = ruta_lista_aristas(ruta);
        escribir_lista_aristas(g, ruta_aristas);

        std::cout << "== Datos: " << ruta << " ==\n"
                  << "  Barras (vertices)            : " << g.n << '\n'
                  << "  Ramas en el archivo          : " << g.ramas_totales << '\n'
                  << "  Ramas fuera de servicio      : " << g.ramas_fuera_servicio << '\n'
                  << "  Lazos descartados            : " << g.lazos << '\n'
                  << "  Lineas paralelas colapsadas  : " << g.paralelas << '\n'
                  << "  Aristas simples (m)          : " << g.aristas.size() << '\n'
                  << "  Lista de aristas escrita en  : " << ruta_aristas << "\n\n";

        DTree dt(g.n);
        EulerTourTreeHKS hks(g.n);
        for (const auto& [u, v] : g.aristas) {
            dt.insertar(u, v);
            hks.insertar(u, v);
        }

        std::cout << std::fixed << std::setprecision(1);
        std::cout << "== Estructuras construidas ==\n"
                  << "                               D-tree      HKS\n"
                  << "  Componentes conexas        : " << std::setw(8) << dt.num_componentes()
                  << std::setw(9) << hks.num_componentes() << '\n'
                  << "  Aristas del arbol          : " << std::setw(8) << dt.num_aristas_arbol()
                  << std::setw(9) << hks.num_aristas_arbol() << '\n'
                  << "  Aristas fuera del arbol    : " << std::setw(8) << dt.num_aristas_fuera()
                  << std::setw(9) << hks.num_aristas_fuera() << '\n'
                  << "  Memoria estimada (KiB)     : " << std::setw(8) << kib(dt.memoria_bytes())
                  << std::setw(9) << kib(hks.memoria_bytes()) << '\n'
                  << "  Ocurrencias en el recorrido:        -" << std::setw(9) << hks.num_ocurrencias()
                  << "   (2n - componentes = " << 2LL * g.n - hks.num_componentes() << ")\n"
                  << "  Muestras por reemplazo HKS :        -" << std::setw(9) << hks.muestras_por_reemplazo() << "\n\n";

        std::pair<int, int> l1 = linea(g, 6173, 6265), l2 = linea(g, 6092, 6340);
        if (l1.first == -1 || l2.first == -1 || !dt.existe_arista(l1.first, l1.second) ||
            !dt.existe_arista(l2.first, l2.second)) {
            l1 = g.aristas[0];
            l2 = g.aristas[1];
        }
        auto nombre = [&](std::pair<int, int> l) {
            return std::to_string(g.barra_id[static_cast<std::size_t>(l.first)]) + "-" +
                   std::to_string(g.barra_id[static_cast<std::size_t>(l.second)]);
        };
        std::cout << "== Contingencia N-2 de ejemplo: caen " << nombre(l1) << " y " << nombre(l2) << " ==\n";
        dt.eliminar(l1.first, l1.second);
        dt.eliminar(l2.first, l2.second);
        hks.eliminar(l1.first, l1.second);
        hks.eliminar(l2.first, l2.second);
        const bool c_dt = dt.conectados(l2.first, l2.second);
        const bool c_hk = hks.conectados(l2.first, l2.second);
        std::cout << "  conn(" << g.barra_id[static_cast<std::size_t>(l2.first)] << ", "
                  << g.barra_id[static_cast<std::size_t>(l2.second)] << ")  D-tree: "
                  << (c_dt ? "conectados" : "NO conectados") << " | HKS: " << (c_hk ? "conectados" : "NO conectados") << '\n'
                  << "  Componentes tras la caida   D-tree: " << dt.num_componentes()
                  << " | HKS: " << hks.num_componentes()
                  << (dt.num_componentes() > 1 ? "   -> islanding" : "   -> la red sigue conectada") << '\n';
        dt.insertar(l1.first, l1.second);
        dt.insertar(l2.first, l2.second);
        hks.insertar(l1.first, l1.second);
        hks.insertar(l2.first, l2.second);
        std::cout << "  Tras restaurar las lineas   D-tree: " << dt.num_componentes()
                  << " componente(s) | HKS: " << hks.num_componentes() << " componente(s)\n";

        if (c_dt != c_hk || dt.num_componentes() != hks.num_componentes()) {
            std::cerr << "ERROR: las estructuras no coinciden.\n";
            return 1;
        }
        std::cout << "\nAmbas estructuras coinciden.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
