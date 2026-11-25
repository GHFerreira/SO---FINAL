#include "../src/simulador.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void testarBaselinePorJob() {
    Config config;
    const std::vector<std::string> arquivos = {
        "tests/job1_soma.txt",
        "tests/job2_vetor.txt",
        "tests/job3_loop.txt"
    };
    const uint32_t clocksEsperados[] = {58, 59, 73};
    const uint32_t acessosEsperados[] = {8, 9, 23};
    const uint32_t instrucoesEsperadas[] = {5, 6, 17};

    for (std::size_t indice = 0; indice < arquivos.size(); ++indice) {
        ResultadoSimulacao resultado =
            Simulador::executarLote({arquivos[indice]}, config);
        assert(resultado.processos.size() == 1);
        assert(resultado.ciclosPreparacao == 50);
        assert(resultado.makespan == acessosEsperados[indice]);
        assert(resultado.tempoTotal == clocksEsperados[indice]);
        assert(resultado.processos[0].ciclosCarga == 50);
        assert(resultado.processos[0].ciclosExecucao == acessosEsperados[indice]);
        assert(resultado.processos[0].acessosRAM == acessosEsperados[indice]);
        assert(resultado.processos[0].instrucoesExecutadas ==
               instrucoesEsperadas[indice]);
        assert(resultado.processos[0].cicloFim == acessosEsperados[indice]);
        assert(resultado.processos[0].ciclosEspera == 0);
        assert(resultado.processos[0].estado == EstadoProcesso::FINALIZADO);
    }
}

void testarFilaFCFSEMemoriaIsolada() {
    Config config;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"tests/job1_soma.txt", "tests/job2_vetor.txt", "tests/job3_loop.txt"},
        config);

    assert(resultado.processos.size() == 3);
    assert(resultado.ciclosPreparacao == 150);
    assert(resultado.makespan == 40);
    assert(resultado.tempoTotal == 190);
    assert(resultado.processos[0].cicloFim == 8);
    assert(resultado.processos[1].cicloFim == 17);
    assert(resultado.processos[2].cicloFim == 40);
    assert(resultado.processos[0].ciclosEspera == 0);
    assert(resultado.processos[1].ciclosEspera == 8);
    assert(resultado.processos[2].ciclosEspera == 17);
    assert(resultado.processos[0].enderecoBase <
           resultado.processos[1].enderecoBase);
    assert(resultado.processos[1].enderecoBase <
           resultado.processos[2].enderecoBase);
    assert(resultado.processos[0].contexto.R[0] == 5);
    assert(resultado.processos[1].contexto.R[0] == 20);
    assert(resultado.processos[2].contexto.R[3] == 0);
}

void testarCustoDeTrocaEntreProcessos() {
    Config config;
    config.custoTrocaContexto = 2;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"tests/job1_soma.txt", "tests/job2_vetor.txt", "tests/job3_loop.txt"},
        config);

    assert(resultado.ciclosPreparacao == 150);
    assert(resultado.ciclosContexto == 4);
    assert(resultado.makespan == 44);
    assert(resultado.tempoTotal == 194);
    assert(resultado.processos[0].trocasContexto == 0);
    assert(resultado.processos[1].trocasContexto == 1);
    assert(resultado.processos[2].trocasContexto == 1);
    assert(resultado.processos[1].ciclosEspera == 10);
    assert(resultado.processos[2].ciclosEspera == 21);
}

int main() {
    testarBaselinePorJob();
    testarFilaFCFSEMemoriaIsolada();
    testarCustoDeTrocaEntreProcessos();
    std::cout << "Todos os testes do Simulador FCFS passaram!\n";
    return 0;
}
