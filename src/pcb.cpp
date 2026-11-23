#include "pcb.hpp"

PCB::PCB() {
    contexto.R.fill(0);
    contexto.PC = 0;
    contexto.IR = 0;
}

PCB::PCB(int idProcesso, const std::string& nomeProcesso, int prio, uint32_t burst)
    : pid(idProcesso),
      nome(nomeProcesso),
      prioridade(prio),
      burstEstimado(burst) {
    contexto.R.fill(0);
    contexto.PC = 0;
    contexto.IR = 0;
}

void PCB::resetarContadores() {
    estado = EstadoProcesso::NOVO;
    contexto.R.fill(0);
    contexto.PC = 0;
    contexto.IR = 0;
    halt = false;
    erroKernel = false;

    instrucoesExecutadas = 0;
    acessosRAM = 0;
    acessosDisco = 0;
    ciclosExecucao = 0;
    ciclosStall = 0;
    ciclosEspera = 0;
    cicloFim = 0;
    trocasContexto = 0;
    hitsMemoria = 0;
    faltasPagina = 0;
    substituicoesPagina = 0;
}

void PCB::salvarContexto(const RegisterBank& bancoRegistradores, bool estadoHalt) {
    contexto = bancoRegistradores.getContext();
    halt = estadoHalt;
}

void PCB::restaurarContexto(RegisterBank& bancoRegistradores) const {
    bancoRegistradores.setContext(contexto);
}

uint32_t PCB::calcularTempoRetorno() const {
    if (cicloFim < tempoChegada) {
        return 0;
    }
    return cicloFim - tempoChegada;
}

std::string PCB::estadoParaTexto(EstadoProcesso est) {
    switch (est) {
        case EstadoProcesso::NOVO:
            return "NOVO";
        case EstadoProcesso::PRONTO:
            return "PRONTO";
        case EstadoProcesso::EXECUTANDO:
            return "EXECUTANDO";
        case EstadoProcesso::BLOQUEADO:
            return "BLOQUEADO";
        case EstadoProcesso::FINALIZADO:
            return "FINALIZADO";
        case EstadoProcesso::ABORTADO:
            return "ABORTADO";
    }
    return "DESCONHECIDO";
}
