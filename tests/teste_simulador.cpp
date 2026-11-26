#include "../src/simulador.hpp"
#include <cassert>
#include <fstream>
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

void testarMulticoreEContadoresPorNucleo() {
    const std::vector<std::string> arquivos = {
        "tests/job1_soma.txt", "tests/job2_vetor.txt", "tests/job3_loop.txt"
    };
    const uint32_t makespansEsperados[] = {40, 31, 23};
    const uint32_t nucleosTestados[] = {1, 2, 4};

    for (std::size_t caso = 0; caso < 3; ++caso) {
        Config config;
        config.numeroNucleos = nucleosTestados[caso];
        ResultadoSimulacao resultado = Simulador::executarLote(arquivos, config);
        assert(resultado.processos.size() == 3);
        assert(resultado.ciclosPreparacao == 150);
        assert(resultado.makespan == makespansEsperados[caso]);
        assert(resultado.tempoTotal == 150 + makespansEsperados[caso]);
        assert(resultado.contadoresNucleos.size() == nucleosTestados[caso]);
        for (const ContadoresNucleo& contadores : resultado.contadoresNucleos) {
            assert(contadores.ciclosExecucao + contadores.ciclosStall +
                       contadores.ciclosContexto + contadores.ciclosOciosos ==
                   resultado.makespan);
        }
        for (const PCB& processo : resultado.processos) {
            assert(processo.estado == EstadoProcesso::FINALIZADO);
        }
        assert(resultado.processos[0].contexto.R[0] == 5);
        assert(resultado.processos[1].contexto.R[0] == 20);
    }
}

void testarCustoDeContextoMulticore() {
    Config config;
    config.numeroNucleos = 2;
    config.custoTrocaContexto = 2;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"tests/job1_soma.txt", "tests/job2_vetor.txt", "tests/job3_loop.txt"},
        config);

    assert(resultado.makespan == 33);
    assert(resultado.ciclosContexto == 2);
    assert(resultado.tempoTotal == 183);
    assert(resultado.contadoresNucleos[0].ciclosContexto == 2);
    assert(resultado.contadoresNucleos[1].ciclosContexto == 0);
    assert(resultado.processos[2].ciclosContexto == 2);
}

void testarDesempateDeterministicoPorNucleo() {
    Config config;
    config.numeroNucleos = 2;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"tests/job1_soma.txt", "tests/job1_soma.txt", "tests/job1_soma.txt"},
        config);

    assert((resultado.ordemExecucao == std::vector<int>{1, 2, 3}));
    assert(resultado.makespan == 16);
    assert(resultado.contadoresNucleos[0].ciclosExecucao == 16);
    assert(resultado.contadoresNucleos[1].ciclosExecucao == 8);
}

void criarJobDeTeste(const std::string& caminho, int prioridade,
                    uint32_t burst) {
    std::ofstream arquivo(caminho);
    arquivo << "F000\n.priority " << prioridade << "\n.burst " << burst << "\n";
}

void criarJobComNops(const std::string& caminho, uint32_t quantidadeNops) {
    std::ofstream arquivo(caminho);
    for (uint32_t indice = 0; indice < quantidadeNops; ++indice) {
        arquivo << "2000\n";
    }
    arquivo << "F000\n";
}

void testarContencaoBarramentoEArbitragemCircular() {
    criarJobComNops("build/bus_rr_p1.txt", 1);
    criarJobComNops("build/bus_rr_p2.txt", 1);
    const std::vector<std::string> arquivos = {
        "build/bus_rr_p1.txt", "build/bus_rr_p2.txt"
    };

    Config config;
    config.numeroNucleos = 2;
    ResultadoSimulacao semContencao =
        Simulador::executarLote(arquivos, config);
    assert(semContencao.makespan == 2);
    assert(semContencao.processos[0].ciclosContencaoBarramento == 0);
    assert(semContencao.processos[1].ciclosContencaoBarramento == 0);

    config.contencaoBarramento = true;
    ResultadoSimulacao comContencao =
        Simulador::executarLote(arquivos, config);
    assert(comContencao.makespan == 4);
    assert(comContencao.tempoTotal == 104);
    assert(comContencao.processos[0].ciclosContencaoBarramento == 1);
    assert(comContencao.processos[1].ciclosContencaoBarramento == 2);
    assert(comContencao.contadoresNucleos[0].ciclosContencaoBarramento == 1);
    assert(comContencao.contadoresNucleos[1].ciclosContencaoBarramento == 2);
    for (const ContadoresNucleo& contadores : comContencao.contadoresNucleos) {
        assert(contadores.ciclosExecucao + contadores.ciclosStall +
                   contadores.ciclosContexto + contadores.ciclosOciosos ==
               comContencao.makespan);
    }
    for (const PCB& processo : comContencao.processos) {
        assert(processo.calcularTempoRetorno() == processo.ciclosEspera +
                   processo.ciclosExecucao + processo.ciclosStall);
    }

    ResultadoSimulacao repetida =
        Simulador::executarLote(arquivos, config);
    assert(repetida.makespan == comContencao.makespan);
    assert(repetida.ordemExecucao == comContencao.ordemExecucao);
    assert(repetida.processos[0].ciclosContencaoBarramento ==
           comContencao.processos[0].ciclosContencaoBarramento);
    assert(repetida.processos[1].ciclosContencaoBarramento ==
           comContencao.processos[1].ciclosContencaoBarramento);

    config.numeroNucleos = 1;
    ResultadoSimulacao umNucleo =
        Simulador::executarLote({"tests/job1_soma.txt"}, config);
    config.contencaoBarramento = false;
    ResultadoSimulacao umNucleoSemContencao =
        Simulador::executarLote({"tests/job1_soma.txt"}, config);
    assert(umNucleo.makespan == umNucleoSemContencao.makespan);
    assert(umNucleo.tempoTotal == umNucleoSemContencao.tempoTotal);
}

void criarCargaDeTeste(const std::string& prefixo) {
    const int prioridades[] = {3, 2, 1};
    const int rajadas[] = {4, 3, 2};
    const int quantidades[] = {3, 2, 1};
    for (std::size_t indice = 0; indice < 3; ++indice) {
        std::ofstream arquivo("build/" + prefixo + "_p" +
                              std::to_string(indice + 1) + ".txt");
        arquivo << ".priority " << prioridades[indice] << "\n.burst "
                << rajadas[indice] << "\n";
        for (int instrucao = 0; instrucao < quantidades[indice]; ++instrucao) {
            arquivo << "2000\n";
        }
        arquivo << "F000\n";
    }
}

uint32_t obterFim(const ResultadoSimulacao& resultado, int pid) {
    for (const PCB& processo : resultado.processos) {
        if (processo.pid == pid) {
            return processo.cicloFim;
        }
    }
    return 0;
}

void testarGanttsDasPoliticas() {
    const PoliticaEscalonamento politicas[] = {
        PoliticaEscalonamento::FCFS,
        PoliticaEscalonamento::SJN,
        PoliticaEscalonamento::ROUND_ROBIN,
        PoliticaEscalonamento::PRIORIDADE
    };
    const std::vector<std::vector<int>> ordemNaoPreemptiva = {
        {1, 2, 3}, {3, 2, 1}, {1, 2, 3}, {3, 2, 1}
    };
    const std::vector<std::vector<int>> ordemPreemptiva = {
        {1, 2, 3, 1, 2}, {3, 2, 2, 1},
        {1, 2, 3, 1, 2}, {3, 2, 2, 1}
    };
    criarCargaDeTeste("gantt");
    const std::vector<std::string> arquivos = {
        "build/gantt_p1.txt", "build/gantt_p2.txt", "build/gantt_p3.txt"
    };

    for (std::size_t indice = 0; indice < 4; ++indice) {
        Config config;
        config.politica = politicas[indice];
        ResultadoSimulacao resultado = Simulador::executarLote(arquivos, config);
        assert(resultado.ordemExecucao == ordemNaoPreemptiva[indice]);
        assert(resultado.makespan == 9);
        if (indice == 0 || indice == 2) {
            assert(obterFim(resultado, 1) == 4);
            assert(obterFim(resultado, 2) == 7);
            assert(obterFim(resultado, 3) == 9);
        } else {
            assert(obterFim(resultado, 1) == 9);
            assert(obterFim(resultado, 2) == 5);
            assert(obterFim(resultado, 3) == 2);
        }

        config.modo = ModoExecucao::PREEMPTIVO;
        config.quantum = 2;
        resultado = Simulador::executarLote(arquivos, config);
        assert(resultado.ordemExecucao == ordemPreemptiva[indice]);
        assert(resultado.makespan == 9);
        if (indice == 0 || indice == 2) {
            assert(obterFim(resultado, 1) == 8);
            assert(obterFim(resultado, 2) == 9);
            assert(obterFim(resultado, 3) == 6);
        } else {
            assert(obterFim(resultado, 1) == 9);
            assert(obterFim(resultado, 2) == 5);
            assert(obterFim(resultado, 3) == 2);
        }
    }
}

void testarContextoEQuantumNaFronteiraDaInstrucao() {
    Config config;
    config.politica = PoliticaEscalonamento::ROUND_ROBIN;
    config.modo = ModoExecucao::PREEMPTIVO;
    config.quantum = 1;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"tests/job1_soma.txt", "tests/job1_soma.txt"}, config);

    assert(resultado.ordemExecucao.size() > 2);
    assert(resultado.ordemExecucao[0] == 1);
    assert(resultado.ordemExecucao[1] == 2);
    assert(resultado.processos[0].contexto.R[2] == 12);
    assert(resultado.processos[1].contexto.R[2] == 12);
    assert(resultado.processos[0].estado == EstadoProcesso::FINALIZADO);
    assert(resultado.processos[1].estado == EstadoProcesso::FINALIZADO);
}

void testarCustoDeContextoEntrePidsNaPreempcao() {
    criarCargaDeTeste("ctx");
    Config config;
    config.modo = ModoExecucao::PREEMPTIVO;
    config.quantum = 2;
    config.custoTrocaContexto = 1;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"build/ctx_p1.txt", "build/ctx_p2.txt", "build/ctx_p3.txt"}, config);

    assert(resultado.ciclosContexto == 4);
    assert(resultado.makespan == 13);
    assert(resultado.processos[0].ciclosContexto == 1);
    assert(resultado.processos[1].ciclosContexto == 2);
    assert(resultado.processos[2].ciclosContexto == 1);
}

void testarSemCustoAoRetomarMesmoPid() {
    criarCargaDeTeste("sem_ctx_mesmo_pid");
    Config config;
    config.politica = PoliticaEscalonamento::SJN;
    config.modo = ModoExecucao::PREEMPTIVO;
    config.quantum = 2;
    config.custoTrocaContexto = 1;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"build/sem_ctx_mesmo_pid_p1.txt",
         "build/sem_ctx_mesmo_pid_p2.txt",
         "build/sem_ctx_mesmo_pid_p3.txt"}, config);

    assert((resultado.ordemExecucao == std::vector<int>{3, 2, 2, 1}));
    assert(resultado.ciclosContexto == 2);
    assert(resultado.makespan == 11);
}

void testarIntegracaoSJN() {
    criarJobDeTeste("build/sjn_longo.txt", 0, 9);
    criarJobDeTeste("build/sjn_curto.txt", 0, 3);
    criarJobDeTeste("build/sjn_medio.txt", 0, 5);
    Config config;
    config.politica = PoliticaEscalonamento::SJN;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"build/sjn_longo.txt", "build/sjn_curto.txt", "build/sjn_medio.txt"},
        config);
    assert((resultado.ordemExecucao == std::vector<int>{2, 3, 1}));
}

void testarIntegracaoPrioridade() {
    criarJobDeTeste("build/prioridade_baixa.txt", 5, 1);
    criarJobDeTeste("build/prioridade_alta_1.txt", 1, 1);
    criarJobDeTeste("build/prioridade_alta_2.txt", 1, 1);
    Config config;
    config.politica = PoliticaEscalonamento::PRIORIDADE;
    ResultadoSimulacao resultado = Simulador::executarLote(
        {"build/prioridade_baixa.txt", "build/prioridade_alta_1.txt",
         "build/prioridade_alta_2.txt"}, config);
    assert((resultado.ordemExecucao == std::vector<int>{2, 3, 1}));
}

int main() {
    testarBaselinePorJob();
    testarFilaFCFSEMemoriaIsolada();
    testarCustoDeTrocaEntreProcessos();
    testarMulticoreEContadoresPorNucleo();
    testarCustoDeContextoMulticore();
    testarDesempateDeterministicoPorNucleo();
    testarContencaoBarramentoEArbitragemCircular();
    testarIntegracaoSJN();
    testarIntegracaoPrioridade();
    testarGanttsDasPoliticas();
    testarContextoEQuantumNaFronteiraDaInstrucao();
    testarCustoDeContextoEntrePidsNaPreempcao();
    testarSemCustoAoRetomarMesmoPid();
    std::cout << "Todos os testes do Simulador passaram!\n";
    return 0;
}
