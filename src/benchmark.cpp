
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "cargador_matpower.hpp"
#include "dtree.hpp"
#include "ett_hks.hpp"

namespace {

using Reloj = std::chrono::steady_clock;
double us(Reloj::time_point a, Reloj::time_point b) { return std::chrono::duration<double, std::micro>(b - a).count(); }

struct Config {
    int escenarios = 20000, repeticiones = 5, consultas = 8;
    uint64_t semilla = 2026;
    std::string salida = "resultados";
    std::vector<std::string> casos;
};

std::string nombre_caso(const std::string& ruta) {
    std::string s = ruta.substr(ruta.find_last_of("/\\") + 1);
    if (s.size() > 2 && s.substr(s.size() - 2) == ".m") s.resize(s.size() - 2);
    if (s.rfind("case_", 0) == 0) s = s.substr(5);
    else if (s.rfind("case", 0) == 0) s = "IEEE" + s.substr(4);
    return s;
}

struct Grafo {
    int n = 0;
    std::vector<std::pair<int, int>> aristas;
    std::vector<std::vector<std::pair<int, int>>> ady;

    explicit Grafo(const GrafoRed& g) : n(g.n), aristas(g.aristas), ady(static_cast<std::size_t>(g.n)) {
        for (std::size_t i = 0; i < aristas.size(); ++i) {
            ady[aristas[i].first].emplace_back(aristas[i].second, static_cast<int>(i));
            ady[aristas[i].second].emplace_back(aristas[i].first, static_cast<int>(i));
        }
    }

    int componentes(std::vector<int>& c, int sin1 = -1, int sin2 = -1) const {
        c.assign(static_cast<std::size_t>(n), -1);
        std::vector<int> cola;
        int k = 0;
        for (int s = 0; s < n; ++s) {
            if (c[s] != -1) continue;
            cola.assign(1, s);
            c[s] = k;
            for (std::size_t i = 0; i < cola.size(); ++i)
                for (const auto& [w, e] : ady[cola[i]])
                    if (e != sin1 && e != sin2 && c[w] == -1) { c[w] = k; cola.push_back(w); }
            ++k;
        }
        return k;
    }

    void puentes(std::vector<int>& ids, std::vector<int>& lado_menor) const {
        std::vector<int> disc(static_cast<std::size_t>(n), -1), low(static_cast<std::size_t>(n)), sub(static_cast<std::size_t>(n), 1);
        int t = 0;
        struct Marco { int v, ep; std::size_t k; };
        for (int s = 0; s < n; ++s) {
            if (disc[s] != -1) continue;
            std::vector<std::pair<int, int>> pendientes;
            std::vector<Marco> pila{{s, -1, 0}};
            disc[s] = low[s] = t++;
            while (!pila.empty()) {
                Marco& f = pila.back();
                if (f.k < ady[f.v].size()) {
                    const auto [w, e] = ady[f.v][f.k++];
                    if (e == f.ep) continue;
                    if (disc[w] == -1) { disc[w] = low[w] = t++; pila.push_back({w, e, 0}); }
                    else low[f.v] = std::min(low[f.v], disc[w]);
                } else {
                    const Marco h = f;
                    pila.pop_back();
                    if (pila.empty()) continue;
                    const int p = pila.back().v;
                    sub[p] += sub[h.v];
                    low[p] = std::min(low[p], low[h.v]);
                    if (low[h.v] > disc[p]) pendientes.emplace_back(h.ep, sub[h.v]);
                }
            }
            const int tam_comp = sub[s];
            for (const auto& [e, sz] : pendientes) {
                ids.push_back(e);
                lado_menor.push_back(std::min(sz, tam_comp - sz));
            }
        }
    }

    int diametro() const {
        std::vector<int> c;
        const int k = componentes(c);
        std::vector<int> tam(static_cast<std::size_t>(k), 0);
        for (const int x : c) ++tam[x];
        const int mayor = static_cast<int>(std::max_element(tam.begin(), tam.end()) - tam.begin());
        int s = static_cast<int>(std::find(c.begin(), c.end(), mayor) - c.begin());
        int ecc = 0;
        for (int barrido = 0; barrido < 2; ++barrido) {
            std::vector<int> d(static_cast<std::size_t>(n), -1), cola{s};
            d[s] = 0;
            for (std::size_t i = 0; i < cola.size(); ++i)
                for (const auto& [w, e] : ady[cola[i]]) { (void)e; if (d[w] == -1) { d[w] = d[cola[i]] + 1; cola.push_back(w); } }
            s = cola.back();
            ecc = d[s];
        }
        return ecc;
    }
};

struct Escenario {
    int e1, e2;
    std::vector<std::pair<int, int>> consultas;
    std::vector<char> esperado;
    int componentes_extra = 0;
    int barras_aisladas = 0;
    bool isla_e1 = false;
    bool isla_e2 = false;
};

std::vector<Escenario> generar_escenarios(const Grafo& g, const Config& cfg, std::mt19937_64& rng) {
    const long long m = static_cast<long long>(g.aristas.size());
    const long long pares = m * (m - 1) / 2;
    std::vector<std::pair<int, int>> elegidos;
    if (pares <= cfg.escenarios) {
        for (int a = 0; a < m; ++a) for (int b = a + 1; b < m; ++b) elegidos.emplace_back(a, b);
        std::shuffle(elegidos.begin(), elegidos.end(), rng);
    } else {
        std::set<std::pair<int, int>> vistos;
        std::uniform_int_distribution<int> d(0, static_cast<int>(m) - 1);
        while (static_cast<int>(elegidos.size()) < cfg.escenarios) {
            int a = d(rng), b = d(rng);
            if (a == b) continue;
            if (a > b) std::swap(a, b);
            if (vistos.insert({a, b}).second) elegidos.emplace_back(a, b);
        }
    }

    std::vector<int> c0, c1, c_sin1;
    g.componentes(c0);
    std::uniform_int_distribution<int> dv(0, g.n - 1);
    std::uniform_int_distribution<int> moneda(0, 1);
    std::vector<Escenario> esc;
    esc.reserve(elegidos.size());
    for (const auto& [a, b] : elegidos) {
        Escenario s;

        s.e1 = moneda(rng) ? a : b;
        s.e2 = (s.e1 == a) ? b : a;
        const auto [u1, v1] = g.aristas[s.e1];
        const auto [u2, v2] = g.aristas[s.e2];
        s.consultas = {{u1, v1}, {u2, v2}};
        for (int q = 0; q < cfg.consultas; ++q) s.consultas.emplace_back(dv(rng), dv(rng));

        g.componentes(c_sin1, s.e1);
        s.isla_e1 = c_sin1[u1] != c_sin1[v1];
        const int k = g.componentes(c1, s.e1, s.e2);
        s.isla_e2 = c1[u2] != c1[v2];
        for (const auto& [x, y] : s.consultas) s.esperado.push_back(c1[x] == c1[y]);

        std::vector<int> tam(static_cast<std::size_t>(k), 0), origen(static_cast<std::size_t>(k), -1);
        for (int v = 0; v < g.n; ++v) { ++tam[c1[v]]; origen[c1[v]] = c0[v]; }
        std::vector<int> total(static_cast<std::size_t>(k), 0), mayor(static_cast<std::size_t>(k), 0), partes(static_cast<std::size_t>(k), 0);
        for (int c = 0; c < k; ++c) {
            const int o = origen[c];
            total[o] += tam[c];
            mayor[o] = std::max(mayor[o], tam[c]);
            ++partes[o];
        }
        for (int o = 0; o < k; ++o)
            if (partes[o] > 1) { s.barras_aisladas += total[o] - mayor[o]; s.componentes_extra += partes[o] - 1; }
        esc.push_back(std::move(s));
    }
    return esc;
}

struct Medicion {
    std::vector<double> t_elim1, t_elim2, t_consulta, t_ins2, t_ins1;
    std::vector<char> arbol1, arbol2;
    double construccion_ms = 0;
    std::size_t memoria = 0;
};

template <class Estructura>
Medicion ejecutar(const Grafo& g, const std::vector<Escenario>& esc, std::size_t calentamiento,
                  uint64_t semilla, const std::string& nombre_estructura, const std::string& nombre_c) {
    Medicion r;
    const auto t0 = Reloj::now();
    Estructura s = [&]() {
        if constexpr (std::is_same_v<Estructura, EulerTourTreeHKS>) return Estructura(g.n, semilla);
        else return Estructura(g.n);
    }();
    for (const auto& [u, v] : g.aristas) s.insertar(u, v);
    r.construccion_ms = std::chrono::duration<double, std::milli>(Reloj::now() - t0).count();
    r.memoria = s.memoria_bytes();

    const std::size_t S = esc.size();
    for (auto* v : {&r.t_elim1, &r.t_elim2, &r.t_consulta, &r.t_ins2, &r.t_ins1}) v->reserve(S);
    std::vector<char> respuestas;
    for (std::size_t idx = 0; idx < calentamiento + S; ++idx) {
        const Escenario& e = esc[idx < calentamiento ? idx % S : idx - calentamiento];
        const bool medir = idx >= calentamiento;
        const auto [u1, v1] = g.aristas[e.e1];
        const auto [u2, v2] = g.aristas[e.e2];
        const char a1 = s.es_arista_arbol(u1, v1);

        const auto ta = Reloj::now();
        const bool ok1 = s.eliminar(u1, v1);
        const auto tb = Reloj::now();
        const char a2 = s.es_arista_arbol(u2, v2);
        const auto tc = Reloj::now();
        const bool ok2 = s.eliminar(u2, v2);
        const auto td = Reloj::now();
        respuestas.assign(e.consultas.size(), 0);
        const auto te = Reloj::now();
        for (std::size_t q = 0; q < e.consultas.size(); ++q)
            respuestas[q] = s.conectados(e.consultas[q].first, e.consultas[q].second);
        const auto tf = Reloj::now();
        const bool ok3 = s.insertar(u2, v2);
        const auto tg = Reloj::now();
        const bool ok4 = s.insertar(u1, v1);
        const auto th = Reloj::now();

        if (!ok1 || !ok2 || !ok3 || !ok4 || respuestas != e.esperado)
            throw std::runtime_error(nombre_estructura + " respondio mal en " + nombre_c +
                                     " (escenario " + std::to_string(idx) + "): el benchmark se detiene");
        if (!medir) continue;
        r.t_elim1.push_back(us(ta, tb));
        r.t_elim2.push_back(us(tc, td));
        r.t_consulta.push_back(us(te, tf) / static_cast<double>(e.consultas.size()));
        r.t_ins2.push_back(us(tf, tg));
        r.t_ins1.push_back(us(tg, th));
        r.arbol1.push_back(a1);
        r.arbol2.push_back(a2);
    }
    if (s.num_aristas_arbol() + s.num_aristas_fuera() != static_cast<long long>(g.aristas.size()))
        throw std::runtime_error(nombre_estructura + ": la red no quedo restaurada en " + nombre_c);
    return r;
}

struct Stats { double media, mediana, p95, p99, maximo; };
Stats estadisticas(std::vector<double> x) {
    std::sort(x.begin(), x.end());
    auto pct = [&](double p) { return x[static_cast<std::size_t>(p * static_cast<double>(x.size() - 1))]; };
    return {std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size()), pct(0.5), pct(0.95), pct(0.99), x.back()};
}

}

int main(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto valor = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument("Falta el valor de " + a);
            return argv[++i];
        };
        try {
            if (a == "--escenarios") cfg.escenarios = std::stoi(valor());
            else if (a == "--repeticiones") cfg.repeticiones = std::stoi(valor());
            else if (a == "--consultas") cfg.consultas = std::stoi(valor());
            else if (a == "--semilla") cfg.semilla = std::stoull(valor());
            else if (a == "--salida") cfg.salida = valor();
            else cfg.casos.push_back(a);
        } catch (const std::exception& ex) { std::cerr << "Argumento invalido: " << ex.what() << '\n'; return 1; }
    }
    if (cfg.casos.empty())
        cfg.casos = {"data/casos/case118.m", "data/casos/case300.m", "data/ACTIVSg2000.m",
                     "data/casos/case_ACTIVSg10k.m", "data/casos/case_ACTIVSg25k.m"};
    mkdir(cfg.salida.c_str(), 0755);

    std::vector<double> vacios(200000);
    for (double& x : vacios) { const auto a = Reloj::now(); const auto b = Reloj::now(); x = us(a, b); }
    std::nth_element(vacios.begin(), vacios.begin() + 100000, vacios.end());
    const double costo_reloj_us = vacios[100000];
    {
        std::ofstream f(cfg.salida + "/configuracion.txt");
        f << "compilador: " << __VERSION__ << "\n"
          << "optimizacion: -O2 -DNDEBUG\n"
          << "escenarios_por_caso: " << cfg.escenarios << "\n"
          << "repeticiones: " << cfg.repeticiones << "\n"
          << "consultas_aleatorias_por_escenario: " << cfg.consultas << " (+2 de los extremos de las lineas caidas)\n"
          << "semilla: " << cfg.semilla << "\n"
          << "costo_del_cronometro_us: " << costo_reloj_us << "\n";
    }
    std::cout << "Costo del cronometro: " << costo_reloj_us * 1000.0 << " ns por medicion\n";

    std::ofstream f_top(cfg.salida + "/topologia.csv"), f_cortes(cfg.salida + "/cortes_n1.csv"),
        f_res(cfg.salida + "/resumen.csv"), f_cons(cfg.salida + "/construccion.csv"), f_esc(cfg.salida + "/escenarios.csv");
    if (!f_top || !f_res || !f_esc) { std::cerr << "No se pudo escribir en " << cfg.salida << '\n'; return 1; }
    f_top << "caso,n,m,m_sobre_n,componentes,puentes,porcentaje_puentes,diametro,grado_medio,grado_max,hojas,escenarios\n";
    f_cortes << "caso,lado_menor\n";
    f_res << "caso,n,estructura,repeticion,operacion,cantidad,media_us,mediana_us,p95_us,p99_us,max_us\n";
    f_cons << "caso,n,estructura,repeticion,construccion_ms,memoria_bytes\n";
    f_esc << "caso,estructura,escenario,t_elim1_us,t_elim2_us,t_consulta_us,t_ins2_us,t_ins1_us,"
             "arbol1,arbol2,isla_e1,isla_e2,barras_aisladas\n";
    f_esc << std::setprecision(5);
    f_res << std::setprecision(6);

    try {
        for (const std::string& ruta : cfg.casos) {
            const GrafoRed red = cargar_matpower(ruta);
            const Grafo g(red);
            const std::string nc = nombre_caso(ruta);
            std::mt19937_64 rng(cfg.semilla);

            std::vector<int> c, ids, menor;
            const int k = g.componentes(c);
            g.puentes(ids, menor);
            std::vector<int> grado(static_cast<std::size_t>(g.n), 0);
            for (const auto& [u, v] : g.aristas) { ++grado[u]; ++grado[v]; }
            const int hojas = static_cast<int>(std::count(grado.begin(), grado.end(), 1));
            const int gmax = *std::max_element(grado.begin(), grado.end());
            const double m = static_cast<double>(g.aristas.size());
            const std::vector<Escenario> esc = generar_escenarios(g, cfg, rng);
            const int diam = g.diametro();
            f_top << nc << ',' << g.n << ',' << g.aristas.size() << ',' << m / g.n << ',' << k << ',' << ids.size() << ','
                  << 100.0 * static_cast<double>(ids.size()) / m << ',' << diam << ',' << 2.0 * m / g.n << ',' << gmax << ','
                  << hojas << ',' << esc.size() << '\n';
            for (const int x : menor) f_cortes << nc << ',' << x << '\n';

            int con_isla = 0;
            for (const auto& e : esc) con_isla += e.componentes_extra > 0;
            std::cout << "== " << nc << ": n=" << g.n << " m=" << g.aristas.size() << " puentes=" << ids.size()
                      << " escenarios N-2=" << esc.size() << " (" << con_isla << " con islanding)\n";

            const std::size_t calentamiento = std::min<std::size_t>(500, esc.size() / 10);
            for (int rep = 0; rep < cfg.repeticiones; ++rep) {
                for (int turno = 0; turno < 2; ++turno) {
                    const bool es_dtree = (rep % 2 == 0) ? (turno == 0) : (turno == 1);
                    const std::string ne = es_dtree ? "D-tree" : "HKS";
                    const Medicion r = es_dtree
                        ? ejecutar<DTree>(g, esc, calentamiento, 0, ne, nc)
                        : ejecutar<EulerTourTreeHKS>(g, esc, calentamiento, cfg.semilla + static_cast<uint64_t>(rep), ne, nc);

                    f_cons << nc << ',' << g.n << ',' << ne << ',' << rep << ',' << r.construccion_ms << ',' << r.memoria << '\n';
                    std::vector<double> elim(r.t_elim1), ins(r.t_ins2);
                    elim.insert(elim.end(), r.t_elim2.begin(), r.t_elim2.end());
                    ins.insert(ins.end(), r.t_ins1.begin(), r.t_ins1.end());
                    const std::pair<const char*, const std::vector<double>*> ops[] = {
                        {"eliminar", &elim}, {"consultar", &r.t_consulta}, {"insertar", &ins}};
                    for (const auto& [op, v] : ops) {
                        const Stats st = estadisticas(*v);
                        f_res << nc << ',' << g.n << ',' << ne << ',' << rep << ',' << op << ',' << v->size() << ',' << st.media
                              << ',' << st.mediana << ',' << st.p95 << ',' << st.p99 << ',' << st.maximo << '\n';
                    }
                    if (rep == 0)
                        for (std::size_t i = 0; i < esc.size(); ++i)
                            f_esc << nc << ',' << ne << ',' << i << ',' << r.t_elim1[i] << ',' << r.t_elim2[i] << ','
                                  << r.t_consulta[i] << ',' << r.t_ins2[i] << ',' << r.t_ins1[i] << ','
                                  << int(r.arbol1[i]) << ',' << int(r.arbol2[i]) << ',' << int(esc[i].isla_e1) << ','
                                  << int(esc[i].isla_e2) << ',' << esc[i].barras_aisladas << '\n';
                    std::cout << "   rep " << rep + 1 << '/' << cfg.repeticiones << ' ' << std::setw(6) << ne << ": "
                              << std::fixed << std::setprecision(2)
                              << "eliminar " << estadisticas(elim).media << " us | consultar " << estadisticas(r.t_consulta).media
                              << " us | insertar " << estadisticas(ins).media << " us | memoria "
                              << static_cast<double>(r.memoria) / 1024.0 << " KiB\n" << std::defaultfloat;
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "Resultados escritos en " << cfg.salida << "/ (todas las respuestas coincidieron con el BFS)\n";
    return 0;
}
