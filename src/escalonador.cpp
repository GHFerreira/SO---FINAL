#include "escalonador.hpp"
#include <algorithm>
#include <stdexcept>

namespace {
class EscalonadorFCFS : public Escalonador {
public:
    std::size_t selecionarProximo(
        const std::vector<PCB>&,
        const std::vector<std::size_t>& filaProntos) const override {
        if (filaProntos.empty()) {
            throw std::invalid_argument("Fila FCFS vazia.");
        }
        return filaProntos.front();
    }
};

class EscalonadorSJN : public Escalonador {
public:
    std::size_t selecionarProximo(
        const std::vector<PCB>& processos,
        const std::vector<std::size_t>& filaProntos) const override {
        if (filaProntos.empty()) {
            throw std::invalid_argument("Fila SJN vazia.");
        }
        return *std::min_element(
            filaProntos.begin(), filaProntos.end(),
            [&processos](std::size_t indiceA, std::size_t indiceB) {
                const PCB& processoA = processos.at(indiceA);
                const PCB& processoB = processos.at(indiceB);
                const uint32_t restanteA =
                    processoA.burstEstimado > processoA.instrucoesExecutadas
                        ? processoA.burstEstimado - processoA.instrucoesExecutadas
                        : 0;
                const uint32_t restanteB =
                    processoB.burstEstimado > processoB.instrucoesExecutadas
                        ? processoB.burstEstimado - processoB.instrucoesExecutadas
                        : 0;
                if (restanteA != restanteB) {
                    return restanteA < restanteB;
                }
                return processoA.pid < processoB.pid;
            });
    }
};

class EscalonadorRoundRobin : public Escalonador {
public:
    std::size_t selecionarProximo(
        const std::vector<PCB>&,
        const std::vector<std::size_t>& filaProntos) const override {
        if (filaProntos.empty()) {
            throw std::invalid_argument("Fila Round Robin vazia.");
        }
        return filaProntos.front();
    }
};

class EscalonadorPrioridade : public Escalonador {
public:
    std::size_t selecionarProximo(
        const std::vector<PCB>& processos,
        const std::vector<std::size_t>& filaProntos) const override {
        if (filaProntos.empty()) {
            throw std::invalid_argument("Fila de prioridade vazia.");
        }
        return *std::min_element(
            filaProntos.begin(), filaProntos.end(),
            [&processos](std::size_t indiceA, std::size_t indiceB) {
                const PCB& processoA = processos.at(indiceA);
                const PCB& processoB = processos.at(indiceB);
                if (processoA.prioridade != processoB.prioridade) {
                    return processoA.prioridade < processoB.prioridade;
                }
                return processoA.pid < processoB.pid;
            });
    }
};
}

std::unique_ptr<Escalonador> criarEscalonador(PoliticaEscalonamento politica) {
    switch (politica) {
        case PoliticaEscalonamento::FCFS:
            return std::make_unique<EscalonadorFCFS>();
        case PoliticaEscalonamento::SJN:
            return std::make_unique<EscalonadorSJN>();
        case PoliticaEscalonamento::ROUND_ROBIN:
            return std::make_unique<EscalonadorRoundRobin>();
        case PoliticaEscalonamento::PRIORIDADE:
            return std::make_unique<EscalonadorPrioridade>();
    }
    throw std::invalid_argument("Politica de escalonamento desconhecida.");
}
