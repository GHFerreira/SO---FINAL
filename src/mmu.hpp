#ifndef MMU_HPP
#define MMU_HPP

#include "memoria.hpp"
#include <cstdint>
#include <unordered_map>

struct ResultadoAcesso {
    uint16_t enderecoFisico = 0;
    bool faltaPagina = false;
    uint32_t ciclosStall = 0;
};

class MMU {
public:
    virtual ~MMU() = default;

    virtual ResultadoAcesso traduzir(int pid, uint16_t enderecoLogico) = 0;
    virtual uint16_t ler(int pid, uint16_t enderecoLogico, ResultadoAcesso* resultado = nullptr) = 0;
    virtual void escrever(int pid, uint16_t enderecoLogico, uint16_t dado, ResultadoAcesso* resultado = nullptr) = 0;
};

struct RegiaoProcesso {
    uint16_t base = 0;
    uint16_t limite = 0;
};

class MMUBaseLimite : public MMU {
private:
    Memoria& memoria;
    std::unordered_map<int, RegiaoProcesso> regioes;

public:
    explicit MMUBaseLimite(Memoria& memoria);

    void registrarProcesso(int pid, uint16_t base, uint16_t limite);
    void removerProcesso(int pid);
    void limparProcessos();
    bool possuiProcesso(int pid) const;

    ResultadoAcesso traduzir(int pid, uint16_t enderecoLogico) override;
    uint16_t ler(int pid, uint16_t enderecoLogico, ResultadoAcesso* resultado = nullptr) override;
    void escrever(int pid, uint16_t enderecoLogico, uint16_t dado, ResultadoAcesso* resultado = nullptr) override;
};

#endif // MMU_HPP
