
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class AdyacenciaIndexada {
public:
    explicit AdyacenciaIndexada(int n = 0) : lista_(static_cast<std::size_t>(n)) {}

    bool contiene(int u, int v) const { return pos_.count(clave(u, v)) != 0; }

    void agregar(int u, int v) {
        assert(u != v && !contiene(u, v));
        agregar_dirigida(u, v);
        agregar_dirigida(v, u);
        ++num_aristas_;
    }

    void quitar(int u, int v) {
        assert(contiene(u, v));
        quitar_dirigida(u, v);
        quitar_dirigida(v, u);
        --num_aristas_;
    }

    int grado(int u) const { return static_cast<int>(lista_[static_cast<std::size_t>(u)].size()); }
    int vecino(int u, int i) const { return lista_[static_cast<std::size_t>(u)][static_cast<std::size_t>(i)]; }
    const std::vector<int>& vecinos(int u) const { return lista_[static_cast<std::size_t>(u)]; }
    long long num_aristas() const { return num_aristas_; }

    std::size_t memoria_bytes() const {
        std::size_t b = sizeof(*this) + lista_.capacity() * sizeof(std::vector<int>);
        for (const auto& l : lista_) b += l.capacity() * sizeof(int);

        b += pos_.bucket_count() * sizeof(void*);
        b += pos_.size() * (sizeof(uint64_t) + sizeof(int) + 2 * sizeof(void*));
        return b;
    }

private:
    static uint64_t clave(int u, int v) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(u)) << 32) | static_cast<uint32_t>(v);
    }

    void agregar_dirigida(int u, int v) {
        auto& l = lista_[static_cast<std::size_t>(u)];
        pos_[clave(u, v)] = static_cast<int>(l.size());
        l.push_back(v);
    }

    void quitar_dirigida(int u, int v) {
        auto& l = lista_[static_cast<std::size_t>(u)];
        auto it = pos_.find(clave(u, v));
        assert(it != pos_.end());
        const int i = it->second;
        const int ultimo = l.back();
        l[static_cast<std::size_t>(i)] = ultimo;
        pos_[clave(u, ultimo)] = i;
        l.pop_back();
        pos_.erase(clave(u, v));
    }

    std::vector<std::vector<int>> lista_;
    std::unordered_map<uint64_t, int> pos_;
    long long num_aristas_ = 0;
};
