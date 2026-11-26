#include "../src/escalonador.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

std::vector<PCB> criarProcessos() {
    std::vector<PCB> processos;
    processos.emplace_back(1, "job1", 3, 8);
    processos.emplace_back(2, "job2", 1, 4);
    processos.emplace_back(3, "job3", 1, 4);
    return processos;
}

void testarFCFSERoundRobinNaoPreemptivo() {
    const std::vector<PCB> processos = criarProcessos();
    const std::vector<std::size_t> fila = {2, 0, 1};

    auto fcfs = criarEscalonador(PoliticaEscalonamento::FCFS);
    auto roundRobin = criarEscalonador(PoliticaEscalonamento::ROUND_ROBIN);
    assert(fcfs->selecionarProximo(processos, fila) == 2);
    assert(roundRobin->selecionarProximo(processos, fila) == 2);
}

void testarSJNComDesempatePorPID() {
    std::vector<PCB> processos = criarProcessos();
    const std::vector<std::size_t> fila = {0, 2, 1};
    auto sjn = criarEscalonador(PoliticaEscalonamento::SJN);

    assert(sjn->selecionarProximo(processos, fila) == 1);
    processos[0].instrucoesExecutadas = 5;
    assert(sjn->selecionarProximo(processos, fila) == 0);
}

void testarPrioridadeComDesempatePorPID() {
    const std::vector<PCB> processos = criarProcessos();
    const std::vector<std::size_t> fila = {2, 0, 1};
    auto prioridade = criarEscalonador(PoliticaEscalonamento::PRIORIDADE);

    assert(prioridade->selecionarProximo(processos, fila) == 1);
}

void testarFilaVazia() {
    const std::vector<PCB> processos = criarProcessos();
    const std::vector<std::size_t> fila;
    auto fcfs = criarEscalonador(PoliticaEscalonamento::FCFS);
    bool lancouErro = false;
    try {
        fcfs->selecionarProximo(processos, fila);
    } catch (const std::invalid_argument&) {
        lancouErro = true;
    }
    assert(lancouErro);
}

int main() {
    testarFCFSERoundRobinNaoPreemptivo();
    testarSJNComDesempatePorPID();
    testarPrioridadeComDesempatePorPID();
    testarFilaVazia();
    std::cout << "Todos os testes dos escalonadores passaram!\n";
    return 0;
}
