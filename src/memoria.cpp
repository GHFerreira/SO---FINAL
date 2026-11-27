#include "memoria.hpp"
#include <algorithm>
#include <cstddef>
#include <stdexcept>

using namespace std;

Memoria::Memoria(uint32_t tamanho) : ram(tamanho, 0), totalAcessos(0) {}

void Memoria::limpar() {
    fill(ram.begin(), ram.end(), static_cast<uint16_t>(0));
    totalAcessos = 0;
}

uint16_t Memoria::ler(uint16_t enderecoFisico) {
    if (enderecoFisico < OS_RESERVED_MEM) {
        throw runtime_error("SEGFAULT: Tentativa de leitura em area restrita do SO!");
    }
    if (static_cast<size_t>(enderecoFisico) >= ram.size()) {
        throw runtime_error("SEGFAULT: Endereco fisico fora dos limites da RAM!");
    }
    totalAcessos++;
    return ram[enderecoFisico];
}

void Memoria::escrever(uint16_t enderecoFisico, uint16_t dado) {
    if (enderecoFisico < OS_RESERVED_MEM) {
        throw runtime_error("SEGFAULT: Tentativa de escrita em area restrita do SO!");
    }
    if (static_cast<size_t>(enderecoFisico) >= ram.size()) {
        throw runtime_error("SEGFAULT: Endereco fisico fora dos limites da RAM!");
    }
    totalAcessos++;
    ram[enderecoFisico] = dado;
}

uint16_t Memoria::lerKernel(uint16_t enderecoFisico) const {
    if (static_cast<size_t>(enderecoFisico) >= ram.size()) {
        throw out_of_range("Endereco fisico fora dos limites da RAM!");
    }
    return ram[enderecoFisico];
}

void Memoria::escreverKernel(uint16_t enderecoFisico, uint16_t dado) {
    if (static_cast<size_t>(enderecoFisico) >= ram.size()) {
        throw out_of_range("Endereco fisico fora dos limites da RAM!");
    }
    ram[enderecoFisico] = dado;
}

uint32_t Memoria::getTotalAcessos() const {
    return totalAcessos;
}

uint32_t Memoria::getTamanho() const {
    return static_cast<uint32_t>(ram.size());
}
