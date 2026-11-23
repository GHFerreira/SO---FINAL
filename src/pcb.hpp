#ifndef PCB_HPP
#define PCB_HPP

#include "cpu/registers/register_bank.hpp"
#include <cstdint>
#include <string>

enum class EstadoProcesso {
    NOVO,
    PRONTO,
    EXECUTANDO,
    BLOQUEADO,
    FINALIZADO,
    ABORTADO
};

struct PCB {
    int pid = 0;
    std::string nome;
    int prioridade = 0;
    uint32_t burstEstimado = 0;
    uint32_t tempoChegada = 0;

    EstadoProcesso estado = EstadoProcesso::NOVO;
    CPUContext contexto{};
    bool halt = false;
    bool erroKernel = false;

    uint16_t enderecoBase = 0;
    uint16_t tamanhoMemoria = 0;

    uint32_t instrucoesExecutadas = 0;
    uint32_t acessosRAM = 0;
    uint32_t acessosDisco = 0;
    uint32_t ciclosExecucao = 0;
    uint32_t ciclosStall = 0;
    uint32_t ciclosEspera = 0;
    uint32_t cicloFim = 0;
    uint32_t trocasContexto = 0;
    uint32_t hitsMemoria = 0;
    uint32_t faltasPagina = 0;
    uint32_t substituicoesPagina = 0;

    PCB();
    PCB(int idProcesso, const std::string& nomeProcesso, int prio = 0, uint32_t burst = 0);

    void resetarContadores();
    void salvarContexto(const RegisterBank& bancoRegistradores, bool estadoHalt = false);
    void restaurarContexto(RegisterBank& bancoRegistradores) const;
    uint32_t calcularTempoRetorno() const;

    static std::string estadoParaTexto(EstadoProcesso est);
};

#endif // PCB_HPP
