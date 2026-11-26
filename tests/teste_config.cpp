#include "../src/config.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

bool analisar(const std::vector<std::string>& argumentos, Config& config,
              std::vector<std::string>& jobs, std::string& erro) {
    std::vector<char*> ponteiros;
    for (const std::string& argumento : argumentos) {
        ponteiros.push_back(const_cast<char*>(argumento.c_str()));
    }
    return Config::analisarArgumentos(static_cast<int>(ponteiros.size()),
                                      ponteiros.data(), config, jobs, erro);
}

void testarValoresPadrao() {
    Config config;
    assert(config.numeroNucleos == 1);
    assert(config.politica == PoliticaEscalonamento::FCFS);
    assert(config.modo == ModoExecucao::NAO_PREEMPTIVO);
    assert(config.quantum == 1);
    assert(config.custoTrocaContexto == 0);
    assert(config.bitsDeslocamento == 4);
    assert(config.quadros == 8);
    assert(config.substituicao == PoliticaSubstituicao::FIFO);
    assert(!config.contencaoBarramento);
}

void testarLeituraArgumentos() {
    Config config;
    std::vector<std::string> jobs;
    std::string erro;
    const std::vector<std::string> argumentos = {
        "simulador", "--cores=1", "--policy", "fcfs", "--mode=non-preemptive",
        "--quantum", "5", "--ctx-cost=3", "--bits-offset", "4",
        "--frames=8", "--replacement", "fifo", "--bus-contention=false",
        "tests/job1_soma.txt"
    };

    assert(analisar(argumentos, config, jobs, erro));
    assert(erro.empty());
    assert(jobs.size() == 1 && jobs[0] == "tests/job1_soma.txt");
    assert(config.quantum == 5);
    assert(config.custoTrocaContexto == 3);
    assert(config.validarExecucaoFase42(erro));
}

void testarValoresInvalidos() {
    Config config;
    std::vector<std::string> jobs;
    std::string erro;
    const std::vector<std::string> argumentos = {
        "simulador", "--bits-offset", "9"
    };
    assert(!analisar(argumentos, config, jobs, erro));
    assert(!erro.empty());

    erro.clear();
    const std::vector<std::string> politicaInvalida = {
        "simulador", "--policy", "random"
    };
    assert(!analisar(politicaInvalida, config, jobs, erro));
}

void testarNucleosAceitosEValidacaoFutura() {
    Config config;
    std::vector<std::string> jobs;
    std::string erro;
    const std::vector<std::string> argumentos = {
        "simulador", "--cores", "2"
    };
    assert(analisar(argumentos, config, jobs, erro));
    assert(config.validarExecucaoFase42(erro));

    config.numeroNucleos = 0;
    assert(!config.validarExecucaoFase42(erro));
    assert(erro.find("maior que zero") != std::string::npos);

    config.numeroNucleos = 2;
    config.contencaoBarramento = true;
    assert(config.validarExecucaoFase42(erro));

    Config configCli;
    const std::vector<std::string> argumentosComBarramento = {
        "simulador", "--cores=2", "--bus-contention"
    };
    assert(analisar(argumentosComBarramento, configCli, jobs, erro));
    assert(configCli.numeroNucleos == 2);
    assert(configCli.contencaoBarramento);
    assert(configCli.validarExecucaoFase42(erro));
}

void testarCustoDeContextoConfiguravel() {
    Config config;
    config.custoTrocaContexto = 7;
    std::string erro;
    assert(config.validarExecucaoFase42(erro));
}

void testarPoliticasAceitas() {
    const PoliticaEscalonamento politicas[] = {
        PoliticaEscalonamento::FCFS,
        PoliticaEscalonamento::SJN,
        PoliticaEscalonamento::ROUND_ROBIN,
        PoliticaEscalonamento::PRIORIDADE
    };
    for (PoliticaEscalonamento politica : politicas) {
        Config config;
        config.politica = politica;
        std::string erro;
        assert(config.validarExecucaoFase42(erro));
    }

    const std::vector<std::pair<std::string, PoliticaEscalonamento>> valores = {
        {"sjn", PoliticaEscalonamento::SJN},
        {"rr", PoliticaEscalonamento::ROUND_ROBIN},
        {"priority", PoliticaEscalonamento::PRIORIDADE}
    };
    for (const auto& valor : valores) {
        Config config;
        std::vector<std::string> jobs;
        std::string erro;
        std::vector<std::string> argumentos = {
            "simulador", "--policy", valor.first
        };
        assert(analisar(argumentos, config, jobs, erro));
        assert(config.politica == valor.second);
        assert(config.validarExecucaoFase42(erro));
    }

    Config config;
    config.modo = ModoExecucao::PREEMPTIVO;
    std::string erro;
    assert(config.validarExecucaoFase42(erro));
}

void testarModoPreemptivoPelaCLI() {
    Config config;
    std::vector<std::string> jobs;
    std::string erro;
    const std::vector<std::string> argumentos = {
        "simulador", "--mode", "preemptive", "--quantum", "2"
    };
    assert(analisar(argumentos, config, jobs, erro));
    assert(config.modo == ModoExecucao::PREEMPTIVO);
    assert(config.quantum == 2);
    assert(config.validarExecucaoFase42(erro));
}

int main() {
    testarValoresPadrao();
    testarLeituraArgumentos();
    testarValoresInvalidos();
    testarNucleosAceitosEValidacaoFutura();
    testarCustoDeContextoConfiguravel();
    testarPoliticasAceitas();
    testarModoPreemptivoPelaCLI();
    std::cout << "Todos os testes de Config passaram!\n";
    return 0;
}
