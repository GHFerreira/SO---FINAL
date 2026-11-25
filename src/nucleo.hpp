#ifndef NUCLEO_HPP
#define NUCLEO_HPP

#include "cpu/cpu.hpp"
#include "mmu.hpp"
#include <cstdint>
#include <string>

enum class EstadoNucleo {
    OCIOSO,
    EXECUTANDO,
    TROCA_CONTEXTO,
    STALL_FALTA_PAGINA
};

struct ContadoresNucleo {
    uint32_t ciclosExecucao = 0;
    uint32_t ciclosStall = 0;
    uint32_t ciclosContexto = 0;
    uint32_t ciclosOciosos = 0;
    uint32_t instrucoesExecutadas = 0;
};

class Nucleo {
private:
    int identificador;
    CPU cpu;
    EstadoNucleo estado;
    ContadoresNucleo contadores;

public:
    Nucleo(int identificador, MMU& mmu);

    ResultadoPasso executarPasso(int pid);
    void iniciarTrocaContexto(uint32_t ciclos);
    void retomarExecucao();
    void registrarOciosidade(uint32_t ciclos);
    void tornarOcioso();
    void reiniciar();

    CPUContext salvarContexto() const;
    void restaurarContexto(const CPUContext& contexto);
    bool processoFinalizado() const;

    int getIdentificador() const;
    EstadoNucleo getEstado() const;
    const ContadoresNucleo& getContadores() const;

    static std::string estadoParaTexto(EstadoNucleo estado);
};

#endif // NUCLEO_HPP
