#include "escalonador.hpp"
#include <algorithm>
#include <stdexcept>

using namespace std;

namespace {
class EscalonadorFCFS : public Escalonador {
public:
    size_t selecionarProximo(
        const vector<PCB>&,
        const vector<size_t>& filaProntos) const override {
        if (filaProntos.empty()) {
            throw invalid_argument("Fila FCFS vazia.");
        }
        return filaProntos.front();
    }
};

class EscalonadorSJN : public Escalonador {
public:
    size_t selecionarProximo(
        const vector<PCB>& processos,
        const vector<size_t>& filaProntos) const override {
        if (filaProntos.empty()) {
            throw invalid_argument("Fila SJN vazia.");
        }
        return *min_element(
            filaProntos.begin(), filaProntos.end(),
            [&processos](size_t indiceA, size_t indiceB) {
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
    size_t selecionarProximo(
        const vector<PCB>&,
        const vector<size_t>& filaProntos) const override {
        if (filaProntos.empty()) {
            throw invalid_argument("Fila Round Robin vazia.");
        }
        return filaProntos.front();
    }
};

class EscalonadorPrioridade : public Escalonador {
public:
    size_t selecionarProximo(
        const vector<PCB>& processos,
        const vector<size_t>& filaProntos) const override {
        if (filaProntos.empty()) {
            throw invalid_argument("Fila de prioridade vazia.");
        }
        return *min_element(
            filaProntos.begin(), filaProntos.end(),
            [&processos](size_t indiceA, size_t indiceB) {
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

unique_ptr<Escalonador> criarEscalonador(PoliticaEscalonamento politica) {
    switch (politica) {
        case PoliticaEscalonamento::FCFS:
            return make_unique<EscalonadorFCFS>();
        case PoliticaEscalonamento::SJN:
            return make_unique<EscalonadorSJN>();
        case PoliticaEscalonamento::ROUND_ROBIN:
            return make_unique<EscalonadorRoundRobin>();
        case PoliticaEscalonamento::PRIORIDADE:
            return make_unique<EscalonadorPrioridade>();
    }
    throw invalid_argument("Politica de escalonamento desconhecida.");
}
