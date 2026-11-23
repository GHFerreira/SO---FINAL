#include "mmu.hpp"
#include <stdexcept>

MMUBaseLimite::MMUBaseLimite(Memoria& memoria) : memoria(memoria) {}

void MMUBaseLimite::registrarProcesso(int pid, uint16_t base, uint16_t limite) {
    if (base < OS_RESERVED_MEM) {
        throw std::invalid_argument("Base do processo invade area reservada do SO.");
    }
    if (limite == 0) {
        throw std::invalid_argument("Limite do processo deve ser maior que zero.");
    }
    uint32_t fimRegiao = static_cast<uint32_t>(base) + static_cast<uint32_t>(limite);
    if (fimRegiao > memoria.getTamanho()) {
        throw std::invalid_argument("Regiao do processo excede o tamanho da RAM.");
    }

    RegiaoProcesso regiao;
    regiao.base = base;
    regiao.limite = limite;
    regioes[pid] = regiao;
}

void MMUBaseLimite::removerProcesso(int pid) {
    regioes.erase(pid);
}

void MMUBaseLimite::limparProcessos() {
    regioes.clear();
}

bool MMUBaseLimite::possuiProcesso(int pid) const {
    return regioes.find(pid) != regioes.end();
}

ResultadoAcesso MMUBaseLimite::traduzir(int pid, uint16_t enderecoLogico) {
    auto it = regioes.find(pid);
    if (it == regioes.end()) {
        throw std::runtime_error("SEGFAULT: Processo nao registrado na MMU!");
    }

    const RegiaoProcesso& regiao = it->second;
    if (enderecoLogico >= regiao.limite) {
        throw std::runtime_error("SEGFAULT: Endereco logico fora do limite do processo!");
    }

    uint32_t enderecoFisico = static_cast<uint32_t>(regiao.base) + static_cast<uint32_t>(enderecoLogico);
    if (enderecoFisico < OS_RESERVED_MEM || enderecoFisico >= memoria.getTamanho()) {
        throw std::runtime_error("SEGFAULT: Endereco fisico invalido!");
    }

    ResultadoAcesso resultado;
    resultado.enderecoFisico = static_cast<uint16_t>(enderecoFisico);
    resultado.faltaPagina = false;
    resultado.ciclosStall = 0;
    return resultado;
}

uint16_t MMUBaseLimite::ler(int pid, uint16_t enderecoLogico, ResultadoAcesso* resultado) {
    ResultadoAcesso acesso = traduzir(pid, enderecoLogico);
    if (resultado != nullptr) {
        *resultado = acesso;
    }
    return memoria.ler(acesso.enderecoFisico);
}

void MMUBaseLimite::escrever(int pid, uint16_t enderecoLogico, uint16_t dado, ResultadoAcesso* resultado) {
    ResultadoAcesso acesso = traduzir(pid, enderecoLogico);
    if (resultado != nullptr) {
        *resultado = acesso;
    }
    memoria.escrever(acesso.enderecoFisico, dado);
}
