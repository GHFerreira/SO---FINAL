#include "simulador.hpp"

#include "defines.hpp"
#include "escalonador.hpp"
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
#include <memory>
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

std::string politicaParaTexto(PoliticaEscalonamento politica) {
    switch (politica) {
        case PoliticaEscalonamento::FCFS: return "FCFS";
        case PoliticaEscalonamento::SJN: return "SJN";
        case PoliticaEscalonamento::ROUND_ROBIN: return "ROUND ROBIN";
        case PoliticaEscalonamento::PRIORIDADE: return "PRIORIDADE";
    }
    return "DESCONHECIDA";
}

void imprimirResumoProcesso(const PCB& processo) {
    std::cout << "Tempo de retorno no escalonamento: "
              << processo.calcularTempoRetorno() << " ciclos\n";
    std::cout << "Tempo de espera: " << processo.ciclosEspera << " ciclos\n";
    std::cout << "Espera por contencao do barramento: "
              << processo.ciclosContencaoBarramento << " ciclos\n";
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
              << "[SIMULADOR RISC 16-BITS] - "
              << politicaParaTexto(config.politica) << " ("
              << config.numeroNucleos << " NUCLEO(S))\n"
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
        std::cout << "Job " << pid
                  << " carregado na RAM para a fila de prontos.\n";
    }

    resultado.ciclosPreparacao = ClockCore::getCycles();
    std::unique_ptr<Escalonador> escalonador =
        criarEscalonador(config.politica);
    std::vector<std::size_t> filaProntos;
    for (std::size_t indice = 0; indice < resultado.processos.size(); ++indice) {
        filaProntos.push_back(indice);
    }
    enum class EventoNucleo { OCIOSO, PRONTO_PARA_PASSO, PASSO, CONTEXTO };
    struct ExecucaoNucleo {
        std::unique_ptr<Nucleo> nucleo;
        EventoNucleo evento = EventoNucleo::OCIOSO;
        std::size_t processo = 0;
        uint64_t terminaEm = 0;
        uint32_t ciclosQuantum = 0;
        uint32_t acessosBarramentoRestantes = 0;
        uint32_t ciclosStallPosteriores = 0;
        int ultimoPid = -1;
        bool houveErro = false;
        std::string mensagemErro;
    };
    std::vector<ExecucaoNucleo> nucleos(config.numeroNucleos);
    for (std::size_t indice = 0; indice < nucleos.size(); ++indice) {
        nucleos[indice].nucleo = std::make_unique<Nucleo>(
            static_cast<int>(indice), mmu);
    }

    uint64_t tempoAtual = 0;
    std::size_t proximoNucleoBarramento = 0;
    std::size_t processosConcluidos = 0;
    const std::size_t quantidadeProcessos = resultado.processos.size();

    auto finalizarProcesso = [&](ExecucaoNucleo& execucao) {
        PCB& processo = resultado.processos[execucao.processo];
        processo.contexto = execucao.nucleo->salvarContexto();
        processo.halt = execucao.nucleo->processoFinalizado();
        processo.estado = execucao.houveErro
                              ? EstadoProcesso::ABORTADO
                              : EstadoProcesso::FINALIZADO;
        processo.cicloFim = static_cast<uint32_t>(tempoAtual);
        const uint32_t retorno = processo.calcularTempoRetorno();
        const uint32_t tempoOcupado = processo.ciclosExecucao +
                                      processo.ciclosStall;
        processo.ciclosEspera = retorno > tempoOcupado
                                    ? retorno - tempoOcupado
                                    : 0;
        if (execucao.houveErro) {
            processo.erroKernel = true;
            std::cout << "[INTERRUPCAO GERADA PELO HARDWARE] PID "
                      << processo.pid << ": " << execucao.mensagemErro << "\n";
        }
        Reporter::exibirTerminal(criarMetricas(processo));
        Reporter::salvarOutputDat("output.dat", criarMetricas(processo));
        imprimirResumoProcesso(processo);
        std::cout << "===================================================\n";
        ++processosConcluidos;
        execucao.nucleo->tornarOcioso();
        execucao.evento = EventoNucleo::OCIOSO;
        execucao.houveErro = false;
        execucao.mensagemErro.clear();
        execucao.ciclosQuantum = 0;
    };

    auto iniciarPasso = [&](ExecucaoNucleo& execucao) {
        PCB& processo = resultado.processos[execucao.processo];
        try {
            if (processo.instrucoesExecutadas >=
                LIMITE_INSTRUCOES_POR_PROCESSO) {
                throw std::runtime_error(
                    "limite de instrucoes atingido sem HALT");
            }
            ResultadoPasso passo =
                execucao.nucleo->executarPasso(processo.pid);
            processo.acessosRAM += passo.acessosRAM;
            processo.ciclosExecucao += passo.acessosRAM;
            execucao.ciclosQuantum += passo.acessosRAM;
            processo.ciclosStall += passo.ciclosStall;
            if (passo.instrucaoExecutada) {
                ++processo.instrucoesExecutadas;
            } else if (passo.ciclosStall == 0) {
                throw std::runtime_error(
                    "CPU nao avancou nem concluiu a instrucao");
            }
            const uint32_t duracao = passo.acessosRAM + passo.ciclosStall;
            if (duracao == 0) {
                throw std::runtime_error("instrucao sem custo temporal");
            }
            execucao.evento = EventoNucleo::PASSO;
            execucao.ciclosStallPosteriores = passo.ciclosStall;
            if (config.contencaoBarramento) {
                execucao.acessosBarramentoRestantes = passo.acessosRAM;
                execucao.terminaEm = passo.acessosRAM == 0
                                         ? tempoAtual + passo.ciclosStall
                                         : std::numeric_limits<uint64_t>::max();
            } else {
                execucao.acessosBarramentoRestantes = 0;
                execucao.terminaEm = tempoAtual + duracao;
            }
        } catch (const std::exception& erro) {
            execucao.houveErro = true;
            execucao.mensagemErro = erro.what();
            finalizarProcesso(execucao);
        }
    };

    while (processosConcluidos < quantidadeProcessos) {
        // Despachos simultaneos seguem a ordem crescente dos nucleos.
        for (ExecucaoNucleo& execucao : nucleos) {
            if (execucao.evento != EventoNucleo::OCIOSO || filaProntos.empty()) {
                continue;
            }
            const std::size_t indice =
                escalonador->selecionarProximo(resultado.processos, filaProntos);
            filaProntos.erase(
                std::find(filaProntos.begin(), filaProntos.end(), indice));
            PCB& processo = resultado.processos[indice];
            execucao.processo = indice;
            execucao.ciclosQuantum = 0;
            execucao.houveErro = false;
            execucao.mensagemErro.clear();
            processo.estado = EstadoProcesso::EXECUTANDO;
            resultado.ordemExecucao.push_back(processo.pid);
            std::cout << "\nDespachando PID " << processo.pid << " no nucleo "
                      << execucao.nucleo->getIdentificador() << ": "
                      << processo.nome << "\n";

            if (execucao.ultimoPid != -1 && execucao.ultimoPid != processo.pid) {
                ++processo.trocasContexto;
                processo.ciclosContexto += config.custoTrocaContexto;
                execucao.nucleo->iniciarTrocaContexto(config.custoTrocaContexto);
                resultado.ciclosContexto += config.custoTrocaContexto;
            }
            execucao.ultimoPid = processo.pid;
            if (processo.ciclosContexto > 0 &&
                execucao.nucleo->getEstado() == EstadoNucleo::TROCA_CONTEXTO &&
                config.custoTrocaContexto > 0) {
                execucao.evento = EventoNucleo::CONTEXTO;
                execucao.terminaEm = tempoAtual + config.custoTrocaContexto;
            } else {
                execucao.nucleo->restaurarContexto(processo.contexto);
                execucao.evento = EventoNucleo::PRONTO_PARA_PASSO;
            }
        }

        // Cada nucleo inicia no maximo um passo por instante; o resultado
        // funcional do passo e conhecido agora, mas seu custo termina depois.
        for (ExecucaoNucleo& execucao : nucleos) {
            if (execucao.evento == EventoNucleo::PRONTO_PARA_PASSO) {
                iniciarPasso(execucao);
            }
        }

        uint64_t proximoEvento = std::numeric_limits<uint64_t>::max();
        bool haAcessoPendente = false;
        for (const ExecucaoNucleo& execucao : nucleos) {
            if (execucao.evento == EventoNucleo::PASSO &&
                execucao.acessosBarramentoRestantes > 0) {
                haAcessoPendente = true;
            }
            if ((execucao.evento == EventoNucleo::PASSO &&
                 execucao.acessosBarramentoRestantes == 0) ||
                execucao.evento == EventoNucleo::CONTEXTO) {
                proximoEvento = std::min(proximoEvento, execucao.terminaEm);
            }
        }
        const uint64_t instanteArbitragem = tempoAtual + 1;
        if (config.contencaoBarramento && haAcessoPendente) {
            proximoEvento = std::min(proximoEvento, instanteArbitragem);
        }
        if (proximoEvento == std::numeric_limits<uint64_t>::max()) {
            if (processosConcluidos == quantidadeProcessos) {
                break;
            }
            throw std::logic_error(
                "Simulacao sem eventos ativos e com processos pendentes.");
        }

        const uint64_t duracao = proximoEvento - tempoAtual;
        if (duracao > std::numeric_limits<uint32_t>::max()) {
            throw std::overflow_error("duracao da simulacao excedeu o limite");
        }
        ClockCore::avancarTempo(static_cast<uint32_t>(duracao));
        tempoAtual = proximoEvento;

        if (config.contencaoBarramento && haAcessoPendente &&
            tempoAtual == instanteArbitragem) {
            std::size_t nucleoAtendido = nucleos.size();
            for (std::size_t deslocamento = 0; deslocamento < nucleos.size();
                 ++deslocamento) {
                const std::size_t indice =
                    (proximoNucleoBarramento + deslocamento) % nucleos.size();
                if (nucleos[indice].evento == EventoNucleo::PASSO &&
                    nucleos[indice].acessosBarramentoRestantes > 0) {
                    nucleoAtendido = indice;
                    break;
                }
            }
            if (nucleoAtendido == nucleos.size()) {
                throw std::logic_error(
                    "Arbitragem sem solicitacao pendente no barramento.");
            }

            for (std::size_t indice = 0; indice < nucleos.size(); ++indice) {
                ExecucaoNucleo& solicitacao = nucleos[indice];
                if (indice == nucleoAtendido ||
                    solicitacao.evento != EventoNucleo::PASSO ||
                    solicitacao.acessosBarramentoRestantes == 0) {
                    continue;
                }
                PCB& processo = resultado.processos[solicitacao.processo];
                ++processo.ciclosStall;
                ++processo.ciclosContencaoBarramento;
                solicitacao.nucleo->registrarStallBarramento(1);
            }

            ExecucaoNucleo& atendido = nucleos[nucleoAtendido];
            --atendido.acessosBarramentoRestantes;
            proximoNucleoBarramento = (nucleoAtendido + 1) % nucleos.size();
            if (atendido.acessosBarramentoRestantes == 0) {
                atendido.terminaEm = tempoAtual + atendido.ciclosStallPosteriores;
            }
        }

        // Eventos no mesmo ciclo sao concluidos em ordem de nucleo.
        for (ExecucaoNucleo& execucao : nucleos) {
            if ((execucao.evento != EventoNucleo::PASSO &&
                 execucao.evento != EventoNucleo::CONTEXTO) ||
                execucao.terminaEm != tempoAtual ||
                (execucao.evento == EventoNucleo::PASSO &&
                 execucao.acessosBarramentoRestantes > 0)) {
                continue;
            }
            if (execucao.evento == EventoNucleo::CONTEXTO) {
                PCB& processo = resultado.processos[execucao.processo];
                execucao.nucleo->restaurarContexto(processo.contexto);
                execucao.evento = EventoNucleo::PRONTO_PARA_PASSO;
                continue;
            }

            PCB& processo = resultado.processos[execucao.processo];
            processo.contexto = execucao.nucleo->salvarContexto();
            processo.halt = execucao.nucleo->processoFinalizado();
            if (processo.halt) {
                finalizarProcesso(execucao);
            } else if (config.modo == ModoExecucao::PREEMPTIVO &&
                       execucao.ciclosQuantum >= config.quantum &&
                       !filaProntos.empty()) {
                processo.estado = EstadoProcesso::PRONTO;
                filaProntos.push_back(execucao.processo);
                execucao.nucleo->tornarOcioso();
                execucao.evento = EventoNucleo::OCIOSO;
                execucao.ciclosQuantum = 0;
            } else {
                execucao.evento = EventoNucleo::PRONTO_PARA_PASSO;
            }
        }
    }

    resultado.makespan = static_cast<uint32_t>(tempoAtual);
    resultado.contadoresNucleos.reserve(nucleos.size());
    for (ExecucaoNucleo& execucao : nucleos) {
        const ContadoresNucleo& contadores = execucao.nucleo->getContadores();
        const uint32_t ocupado = contadores.ciclosExecucao +
                                 contadores.ciclosStall +
                                 contadores.ciclosContexto;
        const uint32_t ocioso = resultado.makespan > ocupado
                                    ? resultado.makespan - ocupado
                                    : 0;
        execucao.nucleo->registrarOciosidade(ocioso);
        resultado.contadoresNucleos.push_back(execucao.nucleo->getContadores());
    }
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
