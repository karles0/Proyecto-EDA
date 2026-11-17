
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../terceros/httplib.h"
#include "cargador_matpower.hpp"
#include "dtree.hpp"
#include "ett_hks.hpp"

namespace {

struct InfoBarra {
    std::string nombre;
    double kv = 0, lat = 0, lon = 0;
    int subestacion = -1;
    bool con_coordenadas = false;
};

std::vector<std::string> campos_csv(const std::string& linea) {
    std::vector<std::string> out;
    std::string cur;
    bool comillas = false;
    for (std::size_t i = 0; i < linea.size(); ++i) {
        const char c = linea[i];
        if (comillas) {
            if (c == '"' && i + 1 < linea.size() && linea[i + 1] == '"') { cur += '"'; ++i; }
            else if (c == '"') comillas = false;
            else cur += c;
        } else if (c == '"') {
            comillas = true;
        } else if (c == ',') {
            out.push_back(cur);
            cur.clear();
        } else if (c != '\r' && c != '\n') {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

int cargar_barras(const std::string& ruta, const GrafoRed& g, std::vector<InfoBarra>& info) {
    info.assign(static_cast<std::size_t>(g.n), InfoBarra{});
    std::ifstream in(ruta);
    if (!in) return 0;
    std::string linea;
    std::getline(in, linea);
    int n = 0;
    while (std::getline(in, linea)) {
        const auto c = campos_csv(linea);
        if (c.size() < 6) continue;
        const auto it = g.indice.find(std::stoll(c[0]));
        if (it == g.indice.end()) continue;
        InfoBarra& b = info[static_cast<std::size_t>(it->second)];
        b.nombre = c[1];
        b.kv = std::stod(c[2]);
        b.subestacion = std::stoi(c[3]);
        b.lat = std::stod(c[4]);
        b.lon = std::stod(c[5]);
        b.con_coordenadas = true;
        ++n;
    }
    return n;
}

std::string json_texto(const std::string& s) {
    std::string o = "\"";
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); o += buf; }
        else o += ch;
    }
    return o + "\"";
}

std::string json_bool(bool b) { return b ? "true" : "false"; }

class Simulador {
public:
    Simulador(GrafoRed g, std::vector<InfoBarra> info)
        : g_(std::move(g)), info_(std::move(info)), dt_(g_.n), hks_(g_.n),
          activa_(g_.aristas.size(), 1), ady_(static_cast<std::size_t>(g_.n)) {
        for (std::size_t i = 0; i < g_.aristas.size(); ++i) {
            const auto [u, v] = g_.aristas[i];
            indice_arista_[clave(u, v)] = static_cast<int>(i);
            ady_[static_cast<std::size_t>(u)].emplace_back(v, static_cast<int>(i));
            ady_[static_cast<std::size_t>(v)].emplace_back(u, static_cast<int>(i));
            dt_.insertar(u, v);
            hks_.insertar(u, v);
        }
    }

    int n() const { return g_.n; }
    int m() const { return static_cast<int>(g_.aristas.size()); }

    std::string red_json() const {
        std::ostringstream o;
        o.precision(7);
        o << "{\"n\":" << g_.n << ",\"m\":" << m() << ",\"barras\":[";
        for (int v = 0; v < g_.n; ++v) {
            const InfoBarra& b = info_[static_cast<std::size_t>(v)];
            if (v) o << ',';
            o << "{\"id\":" << g_.barra_id[static_cast<std::size_t>(v)] << ",\"nombre\":" << json_texto(b.nombre)
              << ",\"kv\":" << b.kv << ",\"sub\":" << b.subestacion << ",\"lat\":" << b.lat << ",\"lon\":" << b.lon
              << ",\"geo\":" << json_bool(b.con_coordenadas) << '}';
        }
        o << "],\"lineas\":[";
        for (std::size_t i = 0; i < g_.aristas.size(); ++i) {
            if (i) o << ',';
            o << '[' << g_.aristas[i].first << ',' << g_.aristas[i].second << ']';
        }
        o << "],\"ejemplo_n2\":" << ejemplo_n2_json() << '}';
        return o.str();
    }

    std::string estado_json() {

        std::vector<int> r_dt(static_cast<std::size_t>(g_.n)), r_hk(static_cast<std::size_t>(g_.n));
        for (int v = 0; v < g_.n; ++v) {
            r_dt[static_cast<std::size_t>(v)] = dt_.representante(v);
            r_hk[static_cast<std::size_t>(v)] = hks_.representante(v);
        }
        const std::vector<int> p_dt = normalizar(r_dt), p_hk = normalizar(r_hk), p_bfs = normalizar(componentes_bfs());
        const bool coinciden = (p_dt == p_hk);
        const bool oraculo = (p_dt == p_bfs) && (p_hk == p_bfs);

        const int k = p_bfs.empty() ? 0 : *std::max_element(p_bfs.begin(), p_bfs.end()) + 1;
        std::vector<int> tam(static_cast<std::size_t>(k), 0);
        for (const int c : p_bfs) ++tam[static_cast<std::size_t>(c)];
        std::vector<int> orden(static_cast<std::size_t>(k));
        std::iota(orden.begin(), orden.end(), 0);
        std::stable_sort(orden.begin(), orden.end(), [&](int a, int b) { return tam[a] > tam[b]; });
        std::vector<int> rango(static_cast<std::size_t>(k));
        for (int i = 0; i < k; ++i) rango[static_cast<std::size_t>(orden[static_cast<std::size_t>(i)])] = i;

        std::ostringstream o;
        o << "{\"caidas\":[";
        bool primero = true;
        for (std::size_t i = 0; i < activa_.size(); ++i)
            if (!activa_[i]) { o << (primero ? "" : ",") << i; primero = false; }
        o << "],\"componentes\":" << k << ",\"tamanos\":[";
        for (int i = 0; i < k; ++i) o << (i ? "," : "") << tam[static_cast<std::size_t>(orden[static_cast<std::size_t>(i)])];
        o << "],\"componente\":[";
        for (int v = 0; v < g_.n; ++v) o << (v ? "," : "") << rango[static_cast<std::size_t>(p_bfs[static_cast<std::size_t>(v)])];
        o << "],\"coinciden\":" << json_bool(coinciden) << ",\"oraculo\":" << json_bool(oraculo)
          << ",\"dtree\":{\"componentes\":" << dt_.num_componentes() << ",\"aristas_arbol\":" << dt_.num_aristas_arbol()
          << ",\"aristas_fuera\":" << dt_.num_aristas_fuera() << ",\"memoria\":" << dt_.memoria_bytes() << '}'
          << ",\"hks\":{\"componentes\":" << hks_.num_componentes() << ",\"aristas_arbol\":" << hks_.num_aristas_arbol()
          << ",\"aristas_fuera\":" << hks_.num_aristas_fuera() << ",\"memoria\":" << hks_.memoria_bytes()
          << ",\"ocurrencias\":" << hks_.num_ocurrencias() << "}}";
        return o.str();
    }

    std::string cambiar_linea(int i, bool caer, std::string& error) {
        if (i < 0 || i >= m()) { error = "La linea " + std::to_string(i) + " no existe"; return ""; }
        if (caer && !activa_[static_cast<std::size_t>(i)]) { error = "La linea ya esta caida"; return ""; }
        if (!caer && activa_[static_cast<std::size_t>(i)]) { error = "La linea ya esta en servicio"; return ""; }
        const auto [u, v] = g_.aristas[static_cast<std::size_t>(i)];

        const bool arbol_dt = dt_.es_arista_arbol(u, v), arbol_hk = hks_.es_arista_arbol(u, v);
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok_dt = caer ? dt_.eliminar(u, v) : dt_.insertar(u, v);
        const auto t1 = std::chrono::steady_clock::now();
        const bool ok_hk = caer ? hks_.eliminar(u, v) : hks_.insertar(u, v);
        const auto t2 = std::chrono::steady_clock::now();
        if (!ok_dt || !ok_hk) { error = "Una estructura rechazo la operacion"; return ""; }
        activa_[static_cast<std::size_t>(i)] = caer ? 0 : 1;

        const bool siguen_dt = dt_.conectados(u, v);
        const bool siguen_hk = hks_.conectados(u, v);
        const auto us = [](auto a, auto b) { return std::chrono::duration<double, std::micro>(b - a).count(); };

        std::ostringstream o;
        o.precision(4);
        o << "{\"operacion\":{\"linea\":" << i << ",\"caer\":" << json_bool(caer)
          << ",\"dtree\":{\"arbol\":" << json_bool(arbol_dt) << ",\"siguen_conectados\":" << json_bool(siguen_dt)
          << ",\"us\":" << us(t0, t1) << "},\"hks\":{\"arbol\":" << json_bool(arbol_hk)
          << ",\"siguen_conectados\":" << json_bool(siguen_hk) << ",\"us\":" << us(t1, t2)
          << "}},\"estado\":" << estado_json() << '}';
        return o.str();
    }

    std::string restaurar_todo() {
        for (std::size_t i = 0; i < activa_.size(); ++i)
            if (!activa_[i]) {
                const auto [u, v] = g_.aristas[i];
                dt_.insertar(u, v);
                hks_.insertar(u, v);
                activa_[i] = 1;
            }
        return estado_json();
    }

    std::string consulta(int u, int v, std::string& error) {
        if (u < 0 || u >= g_.n || v < 0 || v >= g_.n) { error = "Barra fuera de rango"; return ""; }
        const auto t0 = std::chrono::steady_clock::now();
        const bool c_dt = dt_.conectados(u, v);
        const auto t1 = std::chrono::steady_clock::now();
        const bool c_hk = hks_.conectados(u, v);
        const auto t2 = std::chrono::steady_clock::now();
        const std::vector<int> c = componentes_bfs();
        const bool c_bfs = c[static_cast<std::size_t>(u)] == c[static_cast<std::size_t>(v)];
        const auto us = [](auto a, auto b) { return std::chrono::duration<double, std::micro>(b - a).count(); };
        std::ostringstream o;
        o.precision(4);
        o << "{\"u\":" << u << ",\"v\":" << v << ",\"dtree\":" << json_bool(c_dt) << ",\"hks\":" << json_bool(c_hk)
          << ",\"oraculo\":" << json_bool(c_bfs) << ",\"us_dtree\":" << us(t0, t1) << ",\"us_hks\":" << us(t1, t2) << '}';
        return o.str();
    }

    std::string puentes_json() const {
        const int n = g_.n;
        std::vector<int> disc(static_cast<std::size_t>(n), -1), low(static_cast<std::size_t>(n), 0), puentes;
        int t = 0;
        struct Marco { int v, arista_padre; std::size_t k; };
        for (int s = 0; s < n; ++s) {
            if (disc[static_cast<std::size_t>(s)] != -1) continue;
            std::vector<Marco> pila{{s, -1, 0}};
            disc[static_cast<std::size_t>(s)] = low[static_cast<std::size_t>(s)] = t++;
            while (!pila.empty()) {
                Marco& f = pila.back();
                const auto& vec = ady_[static_cast<std::size_t>(f.v)];
                if (f.k < vec.size()) {
                    const auto [w, e] = vec[f.k++];
                    if (!activa_[static_cast<std::size_t>(e)] || e == f.arista_padre) continue;
                    if (disc[static_cast<std::size_t>(w)] == -1) {
                        disc[static_cast<std::size_t>(w)] = low[static_cast<std::size_t>(w)] = t++;
                        pila.push_back({w, e, 0});
                    } else {
                        low[static_cast<std::size_t>(f.v)] = std::min(low[static_cast<std::size_t>(f.v)], disc[static_cast<std::size_t>(w)]);
                    }
                } else {
                    const Marco hijo = f;
                    pila.pop_back();
                    if (!pila.empty()) {
                        const int p = pila.back().v;
                        low[static_cast<std::size_t>(p)] = std::min(low[static_cast<std::size_t>(p)], low[static_cast<std::size_t>(hijo.v)]);
                        if (low[static_cast<std::size_t>(hijo.v)] > disc[static_cast<std::size_t>(p)]) puentes.push_back(hijo.arista_padre);
                    }
                }
            }
        }
        std::sort(puentes.begin(), puentes.end());
        std::ostringstream o;
        o << "{\"puentes\":[";
        for (std::size_t i = 0; i < puentes.size(); ++i) o << (i ? "," : "") << puentes[i];
        o << "]}";
        return o.str();
    }

private:
    static uint64_t clave(int u, int v) {
        if (u > v) std::swap(u, v);
        return (static_cast<uint64_t>(static_cast<uint32_t>(u)) << 32) | static_cast<uint32_t>(v);
    }

    static std::vector<int> normalizar(const std::vector<int>& r) {
        std::unordered_map<int, int> id;
        std::vector<int> p(r.size());
        for (std::size_t i = 0; i < r.size(); ++i) {
            const auto it = id.find(r[i]);
            if (it == id.end()) { const int nuevo = static_cast<int>(id.size()); id[r[i]] = nuevo; p[i] = nuevo; }
            else p[i] = it->second;
        }
        return p;
    }

    std::vector<int> componentes_bfs() const {
        std::vector<int> c(static_cast<std::size_t>(g_.n), -1), cola;
        int k = 0;
        for (int s = 0; s < g_.n; ++s) {
            if (c[static_cast<std::size_t>(s)] != -1) continue;
            cola.assign(1, s);
            c[static_cast<std::size_t>(s)] = k;
            for (std::size_t i = 0; i < cola.size(); ++i)
                for (const auto& [w, e] : ady_[static_cast<std::size_t>(cola[i])])
                    if (activa_[static_cast<std::size_t>(e)] && c[static_cast<std::size_t>(w)] == -1) {
                        c[static_cast<std::size_t>(w)] = k;
                        cola.push_back(w);
                    }
            ++k;
        }
        return c;
    }

    std::string ejemplo_n2_json() const {
        auto buscar = [&](long long a, long long b) -> int {
            const auto ia = g_.indice.find(a), ib = g_.indice.find(b);
            if (ia == g_.indice.end() || ib == g_.indice.end()) return -1;
            const auto it = indice_arista_.find(clave(ia->second, ib->second));
            return it == indice_arista_.end() ? -1 : it->second;
        };
        const int a = buscar(6173, 6265), b = buscar(6092, 6340);
        if (a < 0 || b < 0) return "null";
        return "[" + std::to_string(a) + "," + std::to_string(b) + "]";
    }

    GrafoRed g_;
    std::vector<InfoBarra> info_;
    DTree dt_;
    EulerTourTreeHKS hks_;
    std::vector<char> activa_;
    std::vector<std::vector<std::pair<int, int>>> ady_;
    std::unordered_map<uint64_t, int> indice_arista_;
};

int entero(const httplib::Request& req, const char* nombre) {
    if (!req.has_param(nombre)) throw std::invalid_argument(std::string("Falta el parametro ") + nombre);
    return std::stoi(req.get_param_value(nombre));
}

void responder_error(httplib::Response& res, int codigo, const std::string& msg) {
    res.status = codigo;
    res.set_content("{\"error\":" + json_texto(msg) + "}", "application/json");
}

}

int main(int argc, char** argv) {
    int puerto = 8080;
    std::string caso = "data/ACTIVSg2000.m", barras = "data/ACTIVSg2000_barras.csv", web = "web";
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string a = argv[i];
        if (a == "--puerto") puerto = std::stoi(argv[i + 1]);
        else if (a == "--caso") caso = argv[i + 1];
        else if (a == "--barras") barras = argv[i + 1];
        else if (a == "--web") web = argv[i + 1];
        else { std::cerr << "Opcion desconocida: " << a << '\n'; return 1; }
    }

    try {
        GrafoRed g = cargar_matpower(caso);
        std::vector<InfoBarra> info;
        const int con_coord = cargar_barras(barras, g, info);
        std::cout << "Red cargada: " << g.n << " barras, " << g.aristas.size() << " lineas\n"
                  << "Coordenadas: " << con_coord << " de " << g.n << " barras ("
                  << (con_coord ? barras : std::string("no se encontro el archivo; se usara una disposicion circular")) << ")\n";

        Simulador sim(std::move(g), std::move(info));
        std::mutex mtx;
        httplib::Server svr;

        if (!svr.set_mount_point("/", web)) {
            std::cerr << "No se encontro la carpeta de la interfaz: " << web << '\n';
            return 1;
        }
        auto json = [](httplib::Response& res, const std::string& s) { res.set_content(s, "application/json"); };

        svr.Get("/api/red", [&](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lk(mtx);
            json(res, sim.red_json());
        });
        svr.Get("/api/estado", [&](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lk(mtx);
            json(res, sim.estado_json());
        });
        auto cambiar = [&](bool caer) {
            return [&, caer](const httplib::Request& req, httplib::Response& res) {
                std::lock_guard<std::mutex> lk(mtx);
                try {
                    std::string error;
                    const std::string r = sim.cambiar_linea(entero(req, "i"), caer, error);
                    if (!error.empty()) return responder_error(res, 409, error);
                    json(res, r);
                } catch (const std::exception& ex) { responder_error(res, 400, ex.what()); }
            };
        };
        svr.Post("/api/caer", cambiar(true));
        svr.Post("/api/restaurar", cambiar(false));
        svr.Post("/api/restaurar_todo", [&](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lk(mtx);
            json(res, sim.restaurar_todo());
        });
        svr.Get("/api/conn", [&](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> lk(mtx);
            try {
                std::string error;
                const std::string r = sim.consulta(entero(req, "u"), entero(req, "v"), error);
                if (!error.empty()) return responder_error(res, 400, error);
                json(res, r);
            } catch (const std::exception& ex) { responder_error(res, 400, ex.what()); }
        });
        svr.Get("/api/puentes", [&](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lk(mtx);
            json(res, sim.puentes_json());
        });

        std::cout << "Interfaz disponible en  http://localhost:" << puerto << "\n"
                  << "Presiona Ctrl+C para detener el servidor." << std::endl;
        if (!svr.listen("127.0.0.1", puerto)) {
            std::cerr << "No se pudo abrir el puerto " << puerto << " (quizas esta en uso; prueba --puerto 8081)\n";
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
