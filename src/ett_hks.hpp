
#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "adyacencia.hpp"

class EulerTourTreeHKS {
public:
    explicit EulerTourTreeHKS(int n, uint64_t semilla = 20250924ULL);

    bool insertar(int u, int v);

    bool eliminar(int u, int v);

    bool conectados(int u, int v) const;

    bool existe_arista(int u, int v) const;

    bool es_arista_arbol(int u, int v) const { return u != v && arista_id_.count(clave(u, v)) != 0; }
    int representante(int v) const { return raiz_treap(activa_[static_cast<std::size_t>(v)]); }
    int num_vertices() const { return n_; }
    int num_componentes() const;
    long long num_aristas_arbol() const { return static_cast<long long>(arista_id_.size()); }
    long long num_aristas_fuera() const { return nte_.num_aristas(); }
    long long num_ocurrencias() const { return ocurrencias_vivas_; }
    int muestras_por_reemplazo() const { return muestras_; }
    std::size_t memoria_bytes() const;

    std::string verificar_invariantes() const;

private:
    struct Ocurrencia {
        int izq = -1, der = -1, padre = -1;
        uint32_t prioridad = 0;
        int tam = 1;
        int activos = 0;
        long long peso = 0;
        int peso_propio = 0;
        int vertice = -1;
        bool activa = false;
        bool viva = false;
        int arista_entrada = -1;
        int arista_salida = -1;
    };
    struct Arista {
        int u = -1, v = -1;
        int desde[2] = {-1, -1};
        int hasta[2] = {-1, -1};
        bool viva = false;
    };

    static uint64_t clave(int u, int v);

    int tam(int x) const { return x == -1 ? 0 : o_[x].tam; }
    void actualizar(int x);
    void actualizar_hacia_arriba(int x);
    std::pair<int, int> dividir_rec(int t, int k);
    int unir_rec(int a, int b);
    std::pair<int, int> dividir(int t, int k);
    int unir(int a, int b);
    int raiz_treap(int x) const;
    int rango(int x) const;
    int primero(int t) const;
    int ultimo(int t) const;

    int nueva_ocurrencia(int vertice, bool activa);
    void liberar_ocurrencia(int x);
    int nueva_arista(int u, int v);
    void borrar_arista(int e);
    void reemplazar_desde(int e, int viejo, int nuevo);
    void reemplazar_hasta(int e, int viejo, int nuevo);

    int fusion_en_union(int A, int B);
    int reroot(int u);
    void link(int u, int v);
    void cut(int e);

    void agregar_no_arbol(int u, int v);
    void quitar_no_arbol(int u, int v);
    void refrescar_peso(int vertice);
    std::pair<int, int> ubicar_por_peso(int t, long long r) const;
    void reemplazar(int u, int v);

    int n_;
    int muestras_;
    std::mt19937_64 rng_;

    std::vector<Ocurrencia> o_;
    std::vector<int> libres_o_;
    long long ocurrencias_vivas_ = 0;
    std::vector<int> activa_;

    std::vector<Arista> ar_;
    std::vector<int> libres_ar_;
    std::unordered_map<uint64_t, int> arista_id_;

    AdyacenciaIndexada nte_;
};
