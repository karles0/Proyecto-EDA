
#include <iostream>
#include "dtree.hpp"
#include "ett_hks.hpp"

int main() {

    DTree dt(5);
    EulerTourTreeHKS hks(5);
    int aristas[][2] = {{0, 1}, {1, 2}, {0, 2}, {2, 3}, {3, 4}};
    for (auto& e : aristas) { dt.insertar(e[0], e[1]); hks.insertar(e[0], e[1]); }

    auto mostrar = [&](const char* caso) {
        std::cout << std::boolalpha << caso
                  << " | conn(0,4)  D-tree=" << dt.conectados(0, 4) << "  HKS=" << hks.conectados(0, 4)
                  << " | componentes  D-tree=" << dt.num_componentes() << "  HKS=" << hks.num_componentes() << '\n';
    };
    mostrar("Inicial              ");
    dt.eliminar(0, 1); hks.eliminar(0, 1);
    mostrar("Cae (0,1)            ");
    dt.eliminar(3, 4); hks.eliminar(3, 4);
    mostrar("Cae tambien (3,4)    ");
}
