
#include "cargador_matpower.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {

std::string limpiar_linea(const std::string& linea, bool& cierra) {
    std::string s = linea;
    const std::size_t com = s.find('%');
    if (com != std::string::npos) s.erase(com);
    cierra = false;
    const std::size_t cb = s.find(']');
    if (cb != std::string::npos) {
        cierra = true;
        s.erase(cb);
    }
    for (char& c : s)
        if (c == ';' || c == '\r' || c == ',') c = ' ';
    return s;
}

bool es_inicio_matriz(const std::string& linea, const std::string& nombre) {

    std::string s = linea;
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
    const std::string patron = "mpc." + nombre + "=[";
    return s.compare(0, patron.size(), patron) == 0;
}

std::vector<std::vector<double>> leer_matriz(std::istream& in, const std::string& nombre) {
    std::vector<std::vector<double>> filas;
    std::string linea;
    bool cierra = false;
    while (!cierra && std::getline(in, linea)) {
        std::istringstream ss(limpiar_linea(linea, cierra));
        std::vector<double> fila;
        double x;
        while (ss >> x) fila.push_back(x);
        if (!ss.eof())
            throw std::runtime_error("Valor no numerico en la matriz mpc." + nombre + ": " + linea);
        if (!fila.empty()) filas.push_back(std::move(fila));
    }
    if (!cierra) throw std::runtime_error("La matriz mpc." + nombre + " no se cierra con ']'");
    return filas;
}

}

GrafoRed cargar_matpower(const std::string& ruta) {
    std::ifstream in(ruta);
    if (!in) throw std::runtime_error("No se pudo abrir el archivo: " + ruta);

    std::vector<std::vector<double>> buses, ramas;
    bool hay_bus = false, hay_branch = false;
    std::string linea;
    while (std::getline(in, linea)) {
        if (!hay_bus && es_inicio_matriz(linea, "bus")) {
            buses = leer_matriz(in, "bus");
            hay_bus = true;
        } else if (!hay_branch && es_inicio_matriz(linea, "branch")) {
            ramas = leer_matriz(in, "branch");
            hay_branch = true;
        }
    }
    if (!hay_bus || !hay_branch)
        throw std::runtime_error("El archivo no contiene las matrices mpc.bus y mpc.branch: " + ruta);

    GrafoRed g;
    for (const auto& f : buses) {
        const long long id = static_cast<long long>(f[0]);
        if (g.indice.count(id)) throw std::runtime_error("Barra duplicada en mpc.bus: " + std::to_string(id));
        g.indice[id] = g.n++;
        g.barra_id.push_back(id);
    }

    std::unordered_set<uint64_t> vistas;
    for (const auto& f : ramas) {
        ++g.ramas_totales;
        if (f.size() < 2) throw std::runtime_error("Fila de mpc.branch con menos de 2 columnas");

        if (f.size() >= 11 && f[10] == 0.0) {
            ++g.ramas_fuera_servicio;
            continue;
        }
        const long long fb = static_cast<long long>(f[0]);
        const long long tb = static_cast<long long>(f[1]);
        auto itf = g.indice.find(fb), itt = g.indice.find(tb);
        if (itf == g.indice.end() || itt == g.indice.end())
            throw std::runtime_error("Rama que referencia una barra inexistente: " +
                                     std::to_string(fb) + " - " + std::to_string(tb));
        int u = itf->second, v = itt->second;
        if (u == v) {
            ++g.lazos;
            continue;
        }
        if (u > v) std::swap(u, v);
        const uint64_t k = (static_cast<uint64_t>(u) << 32) | static_cast<uint32_t>(v);
        if (!vistas.insert(k).second) {
            ++g.paralelas;
            continue;
        }
        g.aristas.emplace_back(u, v);
    }
    return g;
}

void escribir_lista_aristas(const GrafoRed& g, const std::string& ruta) {
    std::ofstream out(ruta);
    if (!out) throw std::runtime_error("No se pudo escribir: " + ruta);
    out << g.n << ' ' << g.aristas.size() << '\n';
    for (const auto& [u, v] : g.aristas)
        out << g.barra_id[static_cast<std::size_t>(u)] << ' ' << g.barra_id[static_cast<std::size_t>(v)] << '\n';
}
