
#include "ett_hks.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

#define HKS_COMPROBAR(cond, msg) \
    do { if (!(cond)) throw std::logic_error(std::string("[HKS] ") + (msg)); } while (0)

EulerTourTreeHKS::EulerTourTreeHKS(int n, uint64_t semilla)
    : n_(n),
      muestras_(n >= 2 ? static_cast<int>(std::floor(std::pow(std::log2(static_cast<double>(n)), 2.0))) : 0),
      rng_(semilla),
      activa_(static_cast<std::size_t>(n), -1),
      nte_(n) {
    o_.reserve(static_cast<std::size_t>(3 * n));

    for (int v = 0; v < n; ++v) activa_[static_cast<std::size_t>(v)] = nueva_ocurrencia(v, true);
}

uint64_t EulerTourTreeHKS::clave(int u, int v) {
    if (u > v) std::swap(u, v);
    return (static_cast<uint64_t>(static_cast<uint32_t>(u)) << 32) | static_cast<uint32_t>(v);
}

void EulerTourTreeHKS::actualizar(int x) {
    Ocurrencia& a = o_[static_cast<std::size_t>(x)];
    a.tam = 1;
    a.activos = a.activa ? 1 : 0;
    a.peso = a.peso_propio;
    if (a.izq != -1) {
        Ocurrencia& l = o_[static_cast<std::size_t>(a.izq)];
        a.tam += l.tam;
        a.activos += l.activos;
        a.peso += l.peso;
        l.padre = x;
    }
    if (a.der != -1) {
        Ocurrencia& r = o_[static_cast<std::size_t>(a.der)];
        a.tam += r.tam;
        a.activos += r.activos;
        a.peso += r.peso;
        r.padre = x;
    }
}

void EulerTourTreeHKS::actualizar_hacia_arriba(int x) {
    while (x != -1) {
        actualizar(x);
        x = o_[static_cast<std::size_t>(x)].padre;
    }
}

std::pair<int, int> EulerTourTreeHKS::dividir_rec(int t, int k) {
    if (t == -1) return {-1, -1};
    Ocurrencia& n = o_[static_cast<std::size_t>(t)];
    if (tam(n.izq) >= k) {
        const auto [a, b] = dividir_rec(n.izq, k);
        o_[static_cast<std::size_t>(t)].izq = b;
        actualizar(t);
        if (a != -1) o_[static_cast<std::size_t>(a)].padre = -1;
        return {a, t};
    }
    const int k_der = k - tam(n.izq) - 1;
    const auto [a, b] = dividir_rec(n.der, k_der);
    o_[static_cast<std::size_t>(t)].der = a;
    actualizar(t);
    if (b != -1) o_[static_cast<std::size_t>(b)].padre = -1;
    return {t, b};
}

int EulerTourTreeHKS::unir_rec(int a, int b) {
    if (a == -1) return b;
    if (b == -1) return a;
    if (o_[static_cast<std::size_t>(a)].prioridad > o_[static_cast<std::size_t>(b)].prioridad) {
        const int d = unir_rec(o_[static_cast<std::size_t>(a)].der, b);
        o_[static_cast<std::size_t>(a)].der = d;
        actualizar(a);
        return a;
    }
    const int i = unir_rec(a, o_[static_cast<std::size_t>(b)].izq);
    o_[static_cast<std::size_t>(b)].izq = i;
    actualizar(b);
    return b;
}

std::pair<int, int> EulerTourTreeHKS::dividir(int t, int k) {
    const auto [a, b] = dividir_rec(t, k);
    if (a != -1) o_[static_cast<std::size_t>(a)].padre = -1;
    if (b != -1) o_[static_cast<std::size_t>(b)].padre = -1;
    return {a, b};
}

int EulerTourTreeHKS::unir(int a, int b) {
    const int r = unir_rec(a, b);
    if (r != -1) o_[static_cast<std::size_t>(r)].padre = -1;
    return r;
}

int EulerTourTreeHKS::raiz_treap(int x) const {
    while (o_[static_cast<std::size_t>(x)].padre != -1) x = o_[static_cast<std::size_t>(x)].padre;
    return x;
}

int EulerTourTreeHKS::rango(int x) const {
    int r = tam(o_[static_cast<std::size_t>(x)].izq);
    while (o_[static_cast<std::size_t>(x)].padre != -1) {
        const int p = o_[static_cast<std::size_t>(x)].padre;
        if (o_[static_cast<std::size_t>(p)].der == x) r += tam(o_[static_cast<std::size_t>(p)].izq) + 1;
        x = p;
    }
    return r;
}

int EulerTourTreeHKS::primero(int t) const {
    while (o_[static_cast<std::size_t>(t)].izq != -1) t = o_[static_cast<std::size_t>(t)].izq;
    return t;
}

int EulerTourTreeHKS::ultimo(int t) const {
    while (o_[static_cast<std::size_t>(t)].der != -1) t = o_[static_cast<std::size_t>(t)].der;
    return t;
}

int EulerTourTreeHKS::nueva_ocurrencia(int vertice, bool activa) {
    int id;
    if (!libres_o_.empty()) {
        id = libres_o_.back();
        libres_o_.pop_back();
    } else {
        id = static_cast<int>(o_.size());
        o_.emplace_back();
    }
    Ocurrencia& x = o_[static_cast<std::size_t>(id)];
    x = Ocurrencia{};
    x.prioridad = static_cast<uint32_t>(rng_());
    x.vertice = vertice;
    x.activa = activa;
    x.viva = true;
    x.peso_propio = activa ? nte_.grado(vertice) : 0;
    actualizar(id);
    ++ocurrencias_vivas_;
    return id;
}

void EulerTourTreeHKS::liberar_ocurrencia(int x) {
    const Ocurrencia& a = o_[static_cast<std::size_t>(x)];
    HKS_COMPROBAR(a.viva && !a.activa, "se intento liberar una ocurrencia activa o ya liberada");
    HKS_COMPROBAR(a.izq == -1 && a.der == -1 && a.padre == -1, "la ocurrencia a liberar no esta aislada");
    HKS_COMPROBAR(a.arista_entrada == -1 && a.arista_salida == -1, "la ocurrencia a liberar sigue referenciada");
    o_[static_cast<std::size_t>(x)] = Ocurrencia{};
    libres_o_.push_back(x);
    --ocurrencias_vivas_;
}

int EulerTourTreeHKS::nueva_arista(int u, int v) {
    int id;
    if (!libres_ar_.empty()) {
        id = libres_ar_.back();
        libres_ar_.pop_back();
    } else {
        id = static_cast<int>(ar_.size());
        ar_.emplace_back();
    }
    ar_[static_cast<std::size_t>(id)] = Arista{};
    ar_[static_cast<std::size_t>(id)].u = u;
    ar_[static_cast<std::size_t>(id)].v = v;
    ar_[static_cast<std::size_t>(id)].viva = true;
    arista_id_[clave(u, v)] = id;
    return id;
}

void EulerTourTreeHKS::borrar_arista(int e) {
    arista_id_.erase(clave(ar_[static_cast<std::size_t>(e)].u, ar_[static_cast<std::size_t>(e)].v));
    ar_[static_cast<std::size_t>(e)] = Arista{};
    libres_ar_.push_back(e);
}

void EulerTourTreeHKS::reemplazar_desde(int e, int viejo, int nuevo) {
    Arista& a = ar_[static_cast<std::size_t>(e)];
    for (int i = 0; i < 2; ++i)
        if (a.desde[i] == viejo) { a.desde[i] = nuevo; return; }
    HKS_COMPROBAR(false, "transicion de origen no encontrada en la arista");
}

void EulerTourTreeHKS::reemplazar_hasta(int e, int viejo, int nuevo) {
    Arista& a = ar_[static_cast<std::size_t>(e)];
    for (int i = 0; i < 2; ++i)
        if (a.hasta[i] == viejo) { a.hasta[i] = nuevo; return; }
    HKS_COMPROBAR(false, "transicion de destino no encontrada en la arista");
}

int EulerTourTreeHKS::fusion_en_union(int A, int B) {
    if (A == -1) return B;
    if (B == -1) return A;
    const int a = ultimo(A), b = primero(B);
    HKS_COMPROBAR(o_[static_cast<std::size_t>(a)].vertice == o_[static_cast<std::size_t>(b)].vertice,
                  "la union no tiene el mismo vertice en la junta");
    HKS_COMPROBAR(o_[static_cast<std::size_t>(a)].arista_salida == -1 &&
                  o_[static_cast<std::size_t>(b)].arista_entrada == -1,
                  "la junta de la union tiene transiciones colgantes");

    if (o_[static_cast<std::size_t>(a)].activa) {
        HKS_COMPROBAR(!o_[static_cast<std::size_t>(b)].activa, "dos ocurrencias activas del mismo vertice");
        const auto [solo_b, resto] = dividir(B, 1);
        HKS_COMPROBAR(solo_b == b, "division inesperada al eliminar la ocurrencia sobrante");
        const int e = o_[static_cast<std::size_t>(b)].arista_salida;
        if (e != -1) reemplazar_desde(e, b, a);
        o_[static_cast<std::size_t>(a)].arista_salida = e;
        o_[static_cast<std::size_t>(b)].arista_salida = -1;
        liberar_ocurrencia(b);
        return unir(A, resto);
    }

    const auto [resto, solo_a] = dividir(A, o_[static_cast<std::size_t>(A)].tam - 1);
    HKS_COMPROBAR(solo_a == a, "division inesperada al eliminar la ocurrencia sobrante");
    const int e = o_[static_cast<std::size_t>(a)].arista_entrada;
    if (e != -1) reemplazar_hasta(e, a, b);
    o_[static_cast<std::size_t>(b)].arista_entrada = e;
    o_[static_cast<std::size_t>(a)].arista_entrada = -1;
    liberar_ocurrencia(a);
    return unir(resto, B);
}

int EulerTourTreeHKS::reroot(int u) {
    const int o = activa_[static_cast<std::size_t>(u)];
    const int t = raiz_treap(o);
    if (o_[static_cast<std::size_t>(primero(t))].vertice == u) return t;

    const auto [L, R] = dividir(t, rango(o));
    const int e = o_[static_cast<std::size_t>(o)].arista_entrada;
    HKS_COMPROBAR(e != -1, "ocurrencia interna sin transicion de entrada");
    const int nueva = nueva_ocurrencia(u, false);
    reemplazar_hasta(e, o, nueva);
    o_[static_cast<std::size_t>(nueva)].arista_entrada = e;
    o_[static_cast<std::size_t>(o)].arista_entrada = -1;
    const int L2 = unir(L, nueva);
    return fusion_en_union(R, L2);
}

void EulerTourTreeHKS::link(int u, int v) {
    const int A = reroot(u);
    const int B = reroot(v);
    HKS_COMPROBAR(A != B, "link entre vertices del mismo arbol");
    const int a_ult = ultimo(A), b_pri = primero(B), b_ult = ultimo(B);
    HKS_COMPROBAR(o_[static_cast<std::size_t>(a_ult)].vertice == u &&
                  o_[static_cast<std::size_t>(b_pri)].vertice == v &&
                  o_[static_cast<std::size_t>(b_ult)].vertice == v,
                  "los recorridos no estan enraizados en los extremos");

    const int e = nueva_arista(u, v);
    const int u_nueva = nueva_ocurrencia(u, false);
    Arista& ar = ar_[static_cast<std::size_t>(e)];
    ar.desde[0] = a_ult; ar.hasta[0] = b_pri;
    ar.desde[1] = b_ult; ar.hasta[1] = u_nueva;
    o_[static_cast<std::size_t>(a_ult)].arista_salida = e;
    o_[static_cast<std::size_t>(b_pri)].arista_entrada = e;
    o_[static_cast<std::size_t>(b_ult)].arista_salida = e;
    o_[static_cast<std::size_t>(u_nueva)].arista_entrada = e;
    unir(unir(A, B), u_nueva);
}

void EulerTourTreeHKS::cut(int e) {
    const Arista ar = ar_[static_cast<std::size_t>(e)];
    const int d = (rango(ar.desde[0]) < rango(ar.desde[1])) ? 0 : 1;
    const int s = 1 - d;
    const int ax = ar.desde[d], by = ar.hasta[d], bp = ar.desde[s], aq = ar.hasta[s];
    HKS_COMPROBAR(o_[static_cast<std::size_t>(ax)].vertice == o_[static_cast<std::size_t>(aq)].vertice &&
                  o_[static_cast<std::size_t>(by)].vertice == o_[static_cast<std::size_t>(bp)].vertice,
                  "transiciones de la arista inconsistentes");

    const auto [X, resto] = dividir(raiz_treap(ax), rango(ax) + 1);
    const auto [medio, Y] = dividir(resto, rango(bp) + 1);
    (void)medio;
    o_[static_cast<std::size_t>(ax)].arista_salida = -1;
    o_[static_cast<std::size_t>(by)].arista_entrada = -1;
    o_[static_cast<std::size_t>(bp)].arista_salida = -1;
    o_[static_cast<std::size_t>(aq)].arista_entrada = -1;
    borrar_arista(e);
    fusion_en_union(X, Y);
}

void EulerTourTreeHKS::refrescar_peso(int vertice) {
    const int o = activa_[static_cast<std::size_t>(vertice)];
    o_[static_cast<std::size_t>(o)].peso_propio = nte_.grado(vertice);
    actualizar_hacia_arriba(o);
}

void EulerTourTreeHKS::agregar_no_arbol(int u, int v) {
    nte_.agregar(u, v);
    refrescar_peso(u);
    refrescar_peso(v);
}

void EulerTourTreeHKS::quitar_no_arbol(int u, int v) {
    nte_.quitar(u, v);
    refrescar_peso(u);
    refrescar_peso(v);
}

std::pair<int, int> EulerTourTreeHKS::ubicar_por_peso(int t, long long r) const {
    int c = t;
    while (true) {
        HKS_COMPROBAR(c != -1, "muestreo fuera del rango de pesos");
        const Ocurrencia& x = o_[static_cast<std::size_t>(c)];
        const long long pi = (x.izq != -1) ? o_[static_cast<std::size_t>(x.izq)].peso : 0;
        if (r <= pi) {
            c = x.izq;
        } else if (r <= pi + x.peso_propio) {
            return {c, static_cast<int>(r - pi)};
        } else {
            r -= pi + x.peso_propio;
            c = x.der;
        }
    }
}

void EulerTourTreeHKS::reemplazar(int u, int v) {
    const int ru = raiz_treap(activa_[static_cast<std::size_t>(u)]);
    const int rv = raiz_treap(activa_[static_cast<std::size_t>(v)]);
    HKS_COMPROBAR(ru != rv, "tras el corte los extremos siguen en el mismo arbol");

    const int T1 = (o_[static_cast<std::size_t>(ru)].peso < o_[static_cast<std::size_t>(rv)].peso) ? ru : rv;
    const long long w = o_[static_cast<std::size_t>(T1)].peso;
    if (w == 0) return;

    if (w >= muestras_) {
        std::uniform_int_distribution<long long> dist(1, w);
        for (int i = 0; i < muestras_; ++i) {
            const auto [x, k] = ubicar_por_peso(T1, dist(rng_));
            const int vx = o_[static_cast<std::size_t>(x)].vertice;
            const int y = nte_.vecino(vx, k - 1);
            if (raiz_treap(activa_[static_cast<std::size_t>(y)]) != T1) {
                quitar_no_arbol(vx, y);
                link(vx, y);
                return;
            }
        }
    }

    std::vector<std::pair<int, int>> cruzan;
    std::vector<int> pila{T1};
    while (!pila.empty()) {
        const int x = pila.back();
        pila.pop_back();
        const Ocurrencia& oc = o_[static_cast<std::size_t>(x)];
        if (oc.activa && oc.peso_propio > 0) {
            for (const int y : nte_.vecinos(oc.vertice))
                if (raiz_treap(activa_[static_cast<std::size_t>(y)]) != T1) cruzan.emplace_back(oc.vertice, y);
        }
        if (oc.izq != -1 && o_[static_cast<std::size_t>(oc.izq)].peso > 0) pila.push_back(oc.izq);
        if (oc.der != -1 && o_[static_cast<std::size_t>(oc.der)].peso > 0) pila.push_back(oc.der);
    }
    if (!cruzan.empty()) {
        const auto [x, y] = cruzan.front();
        quitar_no_arbol(x, y);
        link(x, y);
    }
}

bool EulerTourTreeHKS::existe_arista(int u, int v) const {
    if (u == v) return false;
    return arista_id_.count(clave(u, v)) != 0 || nte_.contiene(u, v);
}

bool EulerTourTreeHKS::conectados(int u, int v) const {
    assert(u >= 0 && u < n_ && v >= 0 && v < n_);
    return raiz_treap(activa_[static_cast<std::size_t>(u)]) == raiz_treap(activa_[static_cast<std::size_t>(v)]);
}

bool EulerTourTreeHKS::insertar(int u, int v) {
    assert(u >= 0 && u < n_ && v >= 0 && v < n_);
    if (u == v || existe_arista(u, v)) return false;
    if (!conectados(u, v))
        link(u, v);
    else
        agregar_no_arbol(u, v);
    return true;
}

bool EulerTourTreeHKS::eliminar(int u, int v) {
    assert(u >= 0 && u < n_ && v >= 0 && v < n_);
    if (u == v) return false;
    if (nte_.contiene(u, v)) {
        quitar_no_arbol(u, v);
        return true;
    }
    const auto it = arista_id_.find(clave(u, v));
    if (it == arista_id_.end()) return false;
    const int e = it->second;
    cut(e);
    reemplazar(u, v);
    return true;
}

int EulerTourTreeHKS::num_componentes() const {
    std::vector<char> visto(o_.size(), 0);
    int c = 0;
    for (int v = 0; v < n_; ++v) {
        const int r = raiz_treap(activa_[static_cast<std::size_t>(v)]);
        if (!visto[static_cast<std::size_t>(r)]) {
            visto[static_cast<std::size_t>(r)] = 1;
            ++c;
        }
    }
    return c;
}

std::size_t EulerTourTreeHKS::memoria_bytes() const {
    std::size_t b = sizeof(*this);
    b += o_.capacity() * sizeof(Ocurrencia) + libres_o_.capacity() * sizeof(int);
    b += activa_.capacity() * sizeof(int);
    b += ar_.capacity() * sizeof(Arista) + libres_ar_.capacity() * sizeof(int);
    b += arista_id_.bucket_count() * sizeof(void*) +
         arista_id_.size() * (sizeof(uint64_t) + sizeof(int) + 2 * sizeof(void*));
    b += nte_.memoria_bytes() - sizeof(nte_);
    return b;
}

std::string EulerTourTreeHKS::verificar_invariantes() const {
    const std::string pre = "[HKS] ";
    const std::size_t N = o_.size();

    long long vivas = 0;
    for (std::size_t i = 0; i < N; ++i) {
        const Ocurrencia& x = o_[i];
        if (!x.viva) continue;
        ++vivas;
        int t = 1, act = x.activa ? 1 : 0;
        long long w = x.peso_propio;
        for (const int c : {x.izq, x.der}) {
            if (c == -1) continue;
            const Ocurrencia& h = o_[static_cast<std::size_t>(c)];
            if (!h.viva) return pre + "hijo liberado en el treap";
            if (h.padre != static_cast<int>(i)) return pre + "puntero al padre inconsistente";
            if (h.prioridad > x.prioridad) return pre + "se viola la propiedad de heap";
            t += h.tam; act += h.activos; w += h.peso;
        }
        if (x.padre != -1) {
            const Ocurrencia& p = o_[static_cast<std::size_t>(x.padre)];
            if (p.izq != static_cast<int>(i) && p.der != static_cast<int>(i)) return pre + "el padre no reconoce al hijo";
        }
        if (t != x.tam || act != x.activos || w != x.peso) return pre + "agregados del treap incorrectos";
        const int esperado = x.activa ? nte_.grado(x.vertice) : 0;
        if (x.peso_propio != esperado) return pre + "peso propio incorrecto";
    }
    if (vivas != ocurrencias_vivas_) return pre + "conteo de ocurrencias vivas incorrecto";

    long long activas = 0;
    for (std::size_t i = 0; i < N; ++i) if (o_[i].viva && o_[i].activa) ++activas;
    if (activas != n_) return pre + "el numero de ocurrencias activas no es n";
    for (int v = 0; v < n_; ++v) {
        const int a = activa_[static_cast<std::size_t>(v)];
        if (a < 0 || static_cast<std::size_t>(a) >= N || !o_[static_cast<std::size_t>(a)].viva ||
            !o_[static_cast<std::size_t>(a)].activa || o_[static_cast<std::size_t>(a)].vertice != v)
            return pre + "ocurrencia activa invalida para el vertice " + std::to_string(v);
    }

    std::vector<int> raiz_de(static_cast<std::size_t>(n_), -1);
    std::vector<int> usos_arista(ar_.size(), 0);
    std::vector<int> visto(static_cast<std::size_t>(n_), -1);
    long long aristas_esperadas = 0;
    for (std::size_t i = 0; i < N; ++i) {
        if (!o_[i].viva || o_[i].padre != -1) continue;
        const int root = static_cast<int>(i);
        std::vector<int> seq, pila;
        int c = root;
        while (c != -1 || !pila.empty()) {
            while (c != -1) { pila.push_back(c); c = o_[static_cast<std::size_t>(c)].izq; }
            c = pila.back(); pila.pop_back();
            seq.push_back(c);
            c = o_[static_cast<std::size_t>(c)].der;
        }
        int nv = 0;
        for (const int x : seq) {
            const int vx = o_[static_cast<std::size_t>(x)].vertice;
            if (visto[static_cast<std::size_t>(vx)] != root) {
                if (raiz_de[static_cast<std::size_t>(vx)] != -1) return pre + "un vertice aparece en dos arboles";
                visto[static_cast<std::size_t>(vx)] = root;
                raiz_de[static_cast<std::size_t>(vx)] = root;
                ++nv;
            }
        }
        if (nv != o_[static_cast<std::size_t>(root)].activos) return pre + "activos de la raiz != vertices del arbol";
        if (static_cast<long long>(seq.size()) != 2LL * nv - 1) return pre + "longitud del recorrido distinta de 2n-1";
        aristas_esperadas += nv - 1;
        const Ocurrencia& f = o_[static_cast<std::size_t>(seq.front())];
        const Ocurrencia& l = o_[static_cast<std::size_t>(seq.back())];
        if (f.vertice != l.vertice) return pre + "el recorrido no empieza y termina en la raiz";
        if (f.arista_entrada != -1 || l.arista_salida != -1) return pre + "extremos del recorrido con transiciones";

        std::vector<int> camino{f.vertice};
        std::vector<char> descendido(static_cast<std::size_t>(n_), 0);
        descendido[static_cast<std::size_t>(f.vertice)] = 1;
        for (std::size_t k = 0; k + 1 < seq.size(); ++k) {
            const Ocurrencia& p = o_[static_cast<std::size_t>(seq[k])];
            const Ocurrencia& q = o_[static_cast<std::size_t>(seq[k + 1])];
            const int e = p.arista_salida;
            if (e == -1 || q.arista_entrada != e) return pre + "transicion sin arista asociada";
            if (static_cast<std::size_t>(e) >= ar_.size() || !ar_[static_cast<std::size_t>(e)].viva)
                return pre + "transicion asociada a una arista muerta";
            const Arista& a = ar_[static_cast<std::size_t>(e)];
            const bool extremos_ok = (a.u == p.vertice && a.v == q.vertice) || (a.v == p.vertice && a.u == q.vertice);
            if (!extremos_ok) return pre + "la transicion no corresponde a los extremos de su arista";
            const bool registrada = (a.desde[0] == seq[k] && a.hasta[0] == seq[k + 1]) ||
                                    (a.desde[1] == seq[k] && a.hasta[1] == seq[k + 1]);
            if (!registrada) return pre + "la arista no registra esta transicion";
            ++usos_arista[static_cast<std::size_t>(e)];
            if (camino.size() >= 2 && camino[camino.size() - 2] == q.vertice) {
                camino.pop_back();
            } else {
                if (descendido[static_cast<std::size_t>(q.vertice)]) return pre + "el recorrido no es de Euler (vertice repetido)";
                descendido[static_cast<std::size_t>(q.vertice)] = 1;
                camino.push_back(q.vertice);
            }
        }
        if (camino.size() != 1) return pre + "el recorrido de Euler no regresa a la raiz";
    }
    for (int v = 0; v < n_; ++v)
        if (raiz_de[static_cast<std::size_t>(v)] == -1) return pre + "vertice fuera de todo arbol";

    long long vivas_ar = 0;
    for (std::size_t e = 0; e < ar_.size(); ++e) {
        if (!ar_[e].viva) continue;
        ++vivas_ar;
        if (usos_arista[e] != 2) return pre + "una arista del arbol no aparece exactamente dos veces";
        const auto it = arista_id_.find(clave(ar_[e].u, ar_[e].v));
        if (it == arista_id_.end() || it->second != static_cast<int>(e)) return pre + "mapa de aristas inconsistente";
    }
    if (vivas_ar != static_cast<long long>(arista_id_.size()) || vivas_ar != aristas_esperadas)
        return pre + "numero de aristas del arbol inconsistente";

    for (int u = 0; u < n_; ++u)
        for (const int v : nte_.vecinos(u)) {
            if (raiz_de[static_cast<std::size_t>(u)] != raiz_de[static_cast<std::size_t>(v)])
                return pre + "arista no-arbol entre arboles distintos";
            if (arista_id_.count(clave(u, v))) return pre + "arista simultaneamente arbol y no-arbol";
        }
    return "";
}
