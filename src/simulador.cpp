#include "simulador.hpp"

#include "defines.hpp"
#include "memoria.hpp"
#include "mmu.hpp"
#include "nucleo.hpp"
#include "../loader/clock_core.hpp"
#include "../loader/parser.hpp"
#include "../loader/ram_loader.hpp"
#include "../loader/reporter.hpp"
#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
const uint32_t LIMITE_INSTRUCOES_POR_PROCESSO = 1000000;

uint32_t calcularTamanhoLogico(const JobData& job) {
    uint32_t tamanho = static_cast<uint32_t>(job.instrucoes.size());
    for (const auto& variavel : job.variaveis) {
        tamanho = std::max(tamanho,
                           static_cast<uint32_t>(variavel.first) + 1);
    }
    return std::max(tamanho, 1u);
}

void imprimirResumoProcesso(const PCB& processo) {
    std::cout << "Tempo de retorno no escalonamento: "
              << processo.calcularTempoRetorno() << " ciclos\n";
    std::cout << "Tempo de espera: " << processo.ciclosEspera << " ciclos\n";
    std::cout << "Trocas de contexto: " << processo.trocasContexto << "\n";
}

JobMetrics criarMetricas(const PCB& processo) {
    JobMetrics metricas{};
    metricas.idJob = processo.pid;
    metricas.nomeJob = processo.nome;
    metricas.instrucoesExecutadas = processo.instrucoesExecutadas;
    metricas.acessosRAM = processo.acessosRAM;
    metricas.acessosDisco = processo.acessosDisco;
    metricas.clockTotal = processo.ciclosCarga + processo.ciclosExecucao +
                          processo.ciclosStall;
    metricas.pcFinal = processo.contexto.PC;
    metricas.irFinal = processo.contexto.IR;
    for (std::size_t indice = 0; indice < processo.contexto.R.size(); ++indice) {
        metricas.registradores[indice] = processo.contexto.R[indice];
    }
    metricas.erroKernel = processo.erroKernel;
    return metricas;
}
}

ResultadoSimulacao Simulador::executarLote(
    const std::vector<std::string>& arquivosJobs, const Config& config) {
    ResultadoSimulacao resultado;
    Memoria memoria(TAM_RAM);
    MMUBaseLimite mmu(memoria);
    uint32_t proximoEnderecoFisico = OS_RESERVED_MEM;

    ClockCore::resetarSimulacao();
    {
        std::ofstream saida("output.dat", std::ios::trunc);
    }

    std::cout << "===================================================\n"
              << "[SIMULADOR RISC 16-BITS] - FCFS (1 NUCLEO)\n"
              << "===================================================\n";

    for (std::size_t indice = 0; indice < arquivosJobs.size(); ++indice) {
        JobData job{};
        const int pid = static_cast<int>(indice) + 1;
        if (!JobParser::carregarArquivo(arquivosJobs[indice], job)) {
            std::cout << "[LOTE] Job " << pid << " ignorado.\n";
            continue;
        }

        const uint32_t tamanhoLogico = calcularTamanhoLogico(job);
        const uint32_t fimRegiao = proximoEnderecoFisico + tamanhoLogico;
        if (tamanhoLogico > std::numeric_limits<uint16_t>::max() ||
            fimRegiao > memoria.getTamanho()) {
            std::cout << "[LOTE] Job " << pid
                      << " excede o espaco de memoria disponivel e foi ignorado.\n";
            continue;
        }

        job.id = pid;
        if (!RAMLoader::carregarNaRAM(
                job, memoria, static_cast<uint16_t>(proximoEnderecoFisico))) {
            std::cout << "[LOTE] Falha ao carregar o job " << pid
                      << " na RAM; job ignorado.\n";
            continue;
        }

        mmu.registrarProcesso(pid, static_cast<uint16_t>(proximoEnderecoFisico),
                              static_cast<uint16_t>(tamanhoLogico));

        PCB processo(pid, job.nomeArquivo, job.prioridade, job.burst);
        processo.estado = EstadoProcesso::PRONTO;
        processo.enderecoBase = static_cast<uint16_t>(proximoEnderecoFisico);
        processo.tamanhoMemoria = static_cast<uint16_t>(tamanhoLogico);
        processo.acessosDisco = 1;
        processo.ciclosCarga = 50;
        resultado.processos.push_back(processo);
        proximoEnderecoFisico = fimRegiao;

        ClockCore::tickDisco(1);
        std::cout << "Job " << pid << " carregado na RAM para a fila FCFS.\n";
    }

    resultado.ciclosPreparacao = ClockCore::getCycles();
    const uint32_t inicioEscalonamento = ClockCore::getGlobalCycles();
    Nucleo nucleo(0, mmu);

    for (std::size_t indice = 0; indice < resultado.processos.size(); ++indice) {
        PCB& processo = resultado.processos[indice];
        processo.estado = EstadoProcesso::EXECUTANDO;

        if (indice > 0) {
            ++processo.trocasContexto;
            processo.ciclosContexto = config.custoTrocaContexto;
            nucleo.iniciarTrocaContexto(config.custoTrocaContexto);
            ClockCore::adicionarCiclos(config.custoTrocaContexto);
            resultado.ciclosContexto += config.custoTrocaContexto;
        }

        nucleo.restaurarContexto(processo.contexto);
        std::cout << "\nDespachando PID " << processo.pid << ": "
                  << processo.nome << "\n";
        bool houveErro = false;
        uint32_t instrucoesDesteProcesso = 0;

        try {
            while (!nucleo.processoFinalizado()) {
                if (instrucoesDesteProcesso >= LIMITE_INSTRUCOES_POR_PROCESSO) {
                    throw std::runtime_error(
                        "limite de instrucoes atingido sem HALT");
                }

                ResultadoPasso passo = nucleo.executarPasso(processo.pid);
                processo.acessosRAM += passo.acessosRAM;
                processo.ciclosExecucao += passo.acessosRAM;
                for (uint32_t acesso = 0; acesso < passo.acessosRAM; ++acesso) {
                    ClockCore::tickRAM();
                }

                processo.ciclosStall += passo.ciclosStall;
                if (passo.ciclosStall > 0) {
                    ClockCore::adicionarCiclos(passo.ciclosStall);
                }

                if (passo.instrucaoExecutada) {
                    ++processo.instrucoesExecutadas;
                    ++instrucoesDesteProcesso;
                } else if (passo.ciclosStall == 0) {
                    throw std::runtime_error(
                        "CPU nao avancou nem concluiu a instrucao");
                }
            }
        } catch (const std::exception& erro) {
            houveErro = true;
            processo.erroKernel = true;
            std::cout << "[INTERRUPCAO GERADA PELO HARDWARE] PID "
                      << processo.pid << ": " << erro.what() << "\n";
        }

        processo.contexto = nucleo.salvarContexto();
        processo.halt = nucleo.processoFinalizado();
        processo.estado = houveErro ? EstadoProcesso::ABORTADO
                                    : EstadoProcesso::FINALIZADO;
        nucleo.tornarOcioso();

        const uint32_t tempoAtual =
            ClockCore::getGlobalCycles() - inicioEscalonamento;
        processo.cicloFim = tempoAtual;
        const uint32_t retorno = processo.calcularTempoRetorno();
        const uint32_t tempoOcupado = processo.ciclosExecucao +
                                      processo.ciclosStall;
        processo.ciclosEspera = retorno > tempoOcupado
                                    ? retorno - tempoOcupado
                                    : 0;

        Reporter::exibirTerminal(criarMetricas(processo));
        Reporter::salvarOutputDat("output.dat", criarMetricas(processo));
        imprimirResumoProcesso(processo);
        std::cout << "===================================================\n";
    }

    resultado.makespan = ClockCore::getGlobalCycles() - inicioEscalonamento;
    resultado.tempoTotal = ClockCore::getGlobalCycles();
    std::cout << "Preparacao do lote: " << resultado.ciclosPreparacao
              << " ciclos\n"
              << "Makespan do escalonamento: " << resultado.makespan
              << " ciclos\n"
              << "Trocas de contexto: " << resultado.ciclosContexto
              << " ciclos\n"
              << "Tempo total (preparacao + escalonamento): "
              << resultado.tempoTotal << " ciclos\n";

    return resultado;
}
