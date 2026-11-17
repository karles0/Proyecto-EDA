
#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <queue>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cargador_matpower.hpp"
#include "dtree.hpp"
#include "ett_hks.hpp"

namespace {

class Oraculo {
public:
    explicit Oraculo(int n) : n_(n), ady_(static_cast<std::size_t>(n)) {}
    bool existe(int u, int v) const { return u != v && ady_[u].count(v); }
    bool insertar(int u, int v) {
        if (u == v || existe(u, v)) return false;
        ady_[u].insert(v); ady_[v].insert(u); ++m_;
        return true;
    }
    bool eliminar(int u, int v) {
        if (!existe(u, v)) return false;
        ady_[u].erase(v); ady_[v].erase(u); --m_;
        return true;
    }
    std::vector<int> etiquetas(int* componentes) const {
        std::vector<int> c(static_cast<std::size_t>(n_), -1);
        int k = 0;
        for (int s = 0; s < n_; ++s) {
            if (c[s] != -1) continue;
            std::queue<int> q; q.push(s); c[s] = k;
            while (!q.empty()) {
                const int x = q.front(); q.pop();
                for (const int y : ady_[x]) if (c[y] == -1) { c[y] = k; q.push(y); }
            }
            ++k;
        }
        if (componentes) *componentes = k;
        return c;
    }
    std::vector<std::pair<int, int>> aristas() const {
        std::vector<std::pair<int, int>> e;
        for (int u = 0; u < n_; ++u) for (const int v : ady_[u]) if (u < v) e.emplace_back(u, v);
        return e;
    }
    long long m() const { return m_; }
private:
    int n_;
    long long m_ = 0;
    std::vector<std::unordered_set<int>> ady_;
};

int fallos = 0;
void fallo(const std::string& msg) {
    ++fallos;
    if (fallos <= 20) std::cerr << "  FALLO: " << msg << '\n';
}

void revisar_invariantes(const DTree& dt, const EulerTourTreeHKS& hk, const std::string& ctx) {
    const std::string a = dt.verificar_invariantes();
    if (!a.empty()) fallo(ctx + " " + a);
    const std::string b = hk.verificar_invariantes();
    if (!b.empty()) fallo(ctx + " " + b);
}

void comparar_estado(DTree& dt, EulerTourTreeHKS& hk, const Oraculo& or_, int n, std::mt19937& rng,
                     const std::string& ctx, bool todos_los_pares) {
    int k = 0;
    const std::vector<int> et = or_.etiquetas(&k);
    if (dt.num_componentes() != k) fallo(ctx + " D-tree: componentes " + std::to_string(dt.num_componentes()) + " vs " + std::to_string(k));
    if (hk.num_componentes() != k) fallo(ctx + " HKS: componentes " + std::to_string(hk.num_componentes()) + " vs " + std::to_string(k));
    if (dt.num_aristas_arbol() + dt.num_aristas_fuera() != or_.m()) fallo(ctx + " D-tree: numero de aristas");
    if (hk.num_aristas_arbol() + hk.num_aristas_fuera() != or_.m()) fallo(ctx + " HKS: numero de aristas");
    if (hk.num_aristas_arbol() != n - k) fallo(ctx + " HKS: aristas del arbol != n - componentes");
    auto probar = [&](int u, int v) {
        const bool esp = et[u] == et[v];
        if (dt.conectados(u, v) != esp) fallo(ctx + " D-tree conn(" + std::to_string(u) + "," + std::to_string(v) + ")");
        if (hk.conectados(u, v) != esp) fallo(ctx + " HKS conn(" + std::to_string(u) + "," + std::to_string(v) + ")");
    };
    if (todos_los_pares) {
        for (int u = 0; u < n; ++u) for (int v = 0; v < n; ++v) probar(u, v);
    } else {
        std::uniform_int_distribution<int> d(0, n - 1);
        for (int i = 0; i < 2000; ++i) probar(d(rng), d(rng));
    }
}

void prueba_aleatoria(int semillas, int ops) {
    for (int s = 1; s <= semillas; ++s) {
        std::mt19937 rng(static_cast<uint32_t>(s));
        const int n = 1 + static_cast<int>(rng() % 40);

        const double p_ins = (s % 3 == 0) ? 0.70 : (s % 3 == 1 ? 0.50 : 0.40);
        DTree dt(n);
        EulerTourTreeHKS hk(n, static_cast<uint64_t>(s) * 7919ULL);
        Oraculo orc(n);
        std::uniform_int_distribution<int> dv(0, n - 1);
        std::uniform_real_distribution<double> dp(0.0, 1.0);
        for (int i = 0; i < ops; ++i) {
            const double p = dp(rng);
            const std::string ctx = "[aleatoria semilla=" + std::to_string(s) + " op=" + std::to_string(i) + "]";
            if (p < p_ins) {
                const int u = dv(rng), v = dv(rng);
                const bool e = orc.insertar(u, v);
                if (dt.insertar(u, v) != e) fallo(ctx + " D-tree insertar");
                if (hk.insertar(u, v) != e) fallo(ctx + " HKS insertar");
            } else if (p < 0.92) {
                const auto ar = orc.aristas();
                int u, v;
                if (!ar.empty() && dp(rng) < 0.9) {
                    const auto& x = ar[rng() % ar.size()];
                    u = x.first; v = x.second;
                    if (rng() & 1) std::swap(u, v);
                } else { u = dv(rng); v = dv(rng); }
                const bool e = orc.eliminar(u, v);
                if (dt.eliminar(u, v) != e) fallo(ctx + " D-tree eliminar");
                if (hk.eliminar(u, v) != e) fallo(ctx + " HKS eliminar");
            } else {
                int k = 0;
                const auto et = orc.etiquetas(&k);
                const int u = dv(rng), v = dv(rng);
                const bool esp = et[u] == et[v];
                if (dt.conectados(u, v) != esp) fallo(ctx + " D-tree conn");
                if (hk.conectados(u, v) != esp) fallo(ctx + " HKS conn");
            }
            revisar_invariantes(dt, hk, ctx);
            if (i % 25 == 0) comparar_estado(dt, hk, orc, n, rng, ctx, true);
            if (fallos) return;
        }
        comparar_estado(dt, hk, orc, n, rng, "[aleatoria fin semilla=" + std::to_string(s) + "]", true);
        if (fallos) return;
    }
}

std::vector<std::pair<int, int>> familia(int tipo, int n) {
    std::vector<std::pair<int, int>> e;
    switch (tipo) {
        case 0: for (int i = 0; i + 1 < n; ++i) e.emplace_back(i, i + 1); break;
        case 1: for (int i = 1; i < n; ++i) e.emplace_back(0, i); break;
        case 2: for (int i = 0; i < n; ++i) e.emplace_back(i, (i + 1) % n); break;
        case 3: for (int i = 0; i < n; ++i) for (int j = i + 1; j < n; ++j) e.emplace_back(i, j); break;
        case 4: { const int w = 6;
                  for (int i = 0; i < n; ++i) { if ((i + 1) % w && i + 1 < n) e.emplace_back(i, i + 1);
                                                if (i + w < n) e.emplace_back(i, i + w); } break; }
        default: for (int i = 0; i + 2 < n; i += 2) {
                     e.emplace_back(i, i + 2); e.emplace_back(i + 1, (i + 3 < n) ? i + 3 : i + 1);
                     e.emplace_back(i, i + 1); } break;
    }
    return e;
}

void prueba_estructurada() {
    const char* nombres[] = {"camino", "estrella", "ciclo", "completo", "grilla", "escalera"};
    for (int tipo = 0; tipo < 6; ++tipo) {
        for (int rep = 0; rep < 8; ++rep) {
            const int n = (tipo == 3) ? 12 : 30;
            std::mt19937 rng(static_cast<uint32_t>(1000 * tipo + rep));
            auto e = familia(tipo, n);
            std::shuffle(e.begin(), e.end(), rng);
            DTree dt(n);
            EulerTourTreeHKS hk(n, static_cast<uint64_t>(rep) + 1);
            Oraculo orc(n);
            const std::string ctx = std::string("[") + nombres[tipo] + " rep=" + std::to_string(rep) + "]";
            for (const auto& [u, v] : e) {
                const bool x = orc.insertar(u, v);
                if (dt.insertar(u, v) != x || hk.insertar(u, v) != x) fallo(ctx + " insercion");
                revisar_invariantes(dt, hk, ctx + " tras insertar");
            }
            comparar_estado(dt, hk, orc, n, rng, ctx + " construido", true);
            std::shuffle(e.begin(), e.end(), rng);
            for (const auto& [u, v] : e) {
                const bool x = orc.eliminar(u, v);
                if (dt.eliminar(u, v) != x || hk.eliminar(u, v) != x) fallo(ctx + " eliminacion");
                revisar_invariantes(dt, hk, ctx + " tras eliminar");
                comparar_estado(dt, hk, orc, n, rng, ctx + " durante destruccion", true);
                if (fallos) return;
            }
        }
    }
}

void prueba_red(const std::string& ruta) {
    const GrafoRed g = cargar_matpower(ruta);
    const int n = g.n;
    DTree dt(n);
    EulerTourTreeHKS hk(n);
    Oraculo orc(n);
    std::mt19937 rng(42);
    for (const auto& [u, v] : g.aristas) {
        orc.insertar(u, v);
        if (!dt.insertar(u, v)) fallo("[red] D-tree rechazo una arista de la red");
        if (!hk.insertar(u, v)) fallo("[red] HKS rechazo una arista de la red");
    }
    revisar_invariantes(dt, hk, "[red construida]");
    comparar_estado(dt, hk, orc, n, rng, "[red construida]", false);
    std::cout << "    red: n=" << n << " m=" << g.aristas.size() << " componentes=" << dt.num_componentes() << '\n';

    auto contingencia = [&](const std::vector<std::pair<int, int>>& caidas, const std::string& ctx) {
        for (const auto& [u, v] : caidas) {
            orc.eliminar(u, v);
            if (!dt.eliminar(u, v) || !hk.eliminar(u, v)) fallo(ctx + " eliminacion rechazada");
        }
        int k = 0;
        const auto et = orc.etiquetas(&k);
        if (dt.num_componentes() != k || hk.num_componentes() != k) fallo(ctx + " numero de componentes");
        for (const auto& [u, v] : caidas) {
            const bool esp = et[u] == et[v];
            if (dt.conectados(u, v) != esp || hk.conectados(u, v) != esp) fallo(ctx + " conn de extremos");
        }
        std::uniform_int_distribution<int> d(0, n - 1);
        for (int i = 0; i < 20; ++i) {
            const int a = d(rng), b = d(rng);
            const bool esp = et[a] == et[b];
            if (dt.conectados(a, b) != esp || hk.conectados(a, b) != esp) fallo(ctx + " conn aleatoria");
        }
        for (const auto& [u, v] : caidas) {
            orc.insertar(u, v);
            if (!dt.insertar(u, v) || !hk.insertar(u, v)) fallo(ctx + " restauracion rechazada");
        }
    };

    int casos = 0;
    for (std::size_t i = 0; i < g.aristas.size(); ++i) {
        contingencia({g.aristas[i]}, "[N-1 #" + std::to_string(i) + "]");
        if (++casos % 400 == 0) revisar_invariantes(dt, hk, "[N-1]");
        if (fallos) return;
    }
    revisar_invariantes(dt, hk, "[tras N-1]");
    std::cout << "    contingencias N-1 verificadas: " << casos << '\n';

    std::uniform_int_distribution<std::size_t> de(0, g.aristas.size() - 1);
    casos = 0;
    for (int i = 0; i < 4000; ++i) {
        const std::size_t a = de(rng), b = de(rng);
        if (a == b) continue;
        contingencia({g.aristas[a], g.aristas[b]}, "[N-2 #" + std::to_string(i) + "]");
        if (++casos % 400 == 0) revisar_invariantes(dt, hk, "[N-2]");
        if (fallos) return;
    }
    revisar_invariantes(dt, hk, "[tras N-2]");
    comparar_estado(dt, hk, orc, n, rng, "[red restaurada]", false);
    for (const auto& [u, v] : g.aristas)
        if (!dt.existe_arista(u, v) || !hk.existe_arista(u, v)) fallo("[red restaurada] falta una arista");
    std::cout << "    contingencias N-2 verificadas: " << casos << '\n';
}

}

int main(int argc, char** argv) {
    const std::string ruta = argc > 1 ? argv[1] : "data/ACTIVSg2000.m";
    try {
        std::cout << "[1/3] Secuencias aleatorias en grafos pequenos...\n";
        prueba_aleatoria(300, 1500);
        std::cout << "      " << (fallos ? "CON FALLOS" : "OK") << '\n';
        if (!fallos) {
            std::cout << "[2/3] Familias estructuradas (camino, estrella, ciclo, completo, grilla, escalera)...\n";
            prueba_estructurada();
            std::cout << "      " << (fallos ? "CON FALLOS" : "OK") << '\n';
        }
        if (!fallos) {
            std::cout << "[3/3] Red electrica " << ruta << "...\n";
            prueba_red(ruta);
            std::cout << "      " << (fallos ? "CON FALLOS" : "OK") << '\n';
        }
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPCION: " << ex.what() << '\n';
        return 2;
    }
    if (fallos) {
        std::cerr << "Total de fallos: " << fallos << '\n';
        return 1;
    }
    std::cout << "Todas las verificaciones pasaron.\n";
    return 0;
}
