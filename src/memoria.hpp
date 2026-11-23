#ifndef MEMORIA_HPP
#define MEMORIA_HPP

#include "defines.hpp"
#include <cstdint>
#include <vector>

class Memoria {
private:
    std::vector<uint16_t> ram;
    uint32_t totalAcessos;

public:
    explicit Memoria(uint32_t tamanho = TAM_RAM);

    void limpar();
    uint16_t ler(uint16_t enderecoFisico);
    void escrever(uint16_t enderecoFisico, uint16_t dado);

    uint16_t lerKernel(uint16_t enderecoFisico) const;
    void escreverKernel(uint16_t enderecoFisico, uint16_t dado);

    uint32_t getTotalAcessos() const;
    uint32_t getTamanho() const;
};

#endif // MEMORIA_HPP
