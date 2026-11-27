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

using namespace std;

namespace {
const uint32_t LIMITE_INSTRUCOES_POR_PROCESSO = 1000000;

uint32_t calcularTamanhoLogico(const JobData& job) {
    uint32_t tamanho = static_cast<uint32_t>(job.instrucoes.size());
    for (const auto& variavel : job.variaveis) {
        tamanho = max(tamanho,
                           static_cast<uint32_t>(variavel.first) + 1);
    }
    return max(tamanho, 1u);
}

string politicaParaTexto(PoliticaEscalonamento politica) {
    switch (politica) {
        case PoliticaEscalonamento::FCFS: return "FCFS";
        case PoliticaEscalonamento::SJN: return "SJN";
        case PoliticaEscalonamento::ROUND_ROBIN: return "ROUND ROBIN";
        case PoliticaEscalonamento::PRIORIDADE: return "PRIORIDADE";
    }
    return "DESCONHECIDA";
}

void imprimirResumoProcesso(const PCB& processo) {
    cout << "Tempo de retorno no escalonamento: "
              << processo.calcularTempoRetorno() << " ciclos\n";
    cout << "Tempo de espera: " << processo.ciclosEspera << " ciclos\n";
    cout << "Espera por contencao do barramento: "
              << processo.ciclosContencaoBarramento << " ciclos\n";
    cout << "Trocas de contexto: " << processo.trocasContexto << "\n";
}

JobMetrics criarMetricas(const PCB& processo) {
    JobMetrics metricas{};
    metricas.idJob = processo.pid;
    metricas.nomeJob = processo.nome;
    metricas.instrucoesExecutadas = processo.instrucoesExecutadas;
    metricas.acessosRAM = processo.acessosRAM;
    metricas.acessosDisco = processo.acessosDisco;
    metricas.hitsMemoria = processo.hitsMemoria;
    metricas.faltasPagina = processo.faltasPagina;
    metricas.substituicoesPagina = processo.substituicoesPagina;
    metricas.clockTotal = processo.ciclosCarga + processo.ciclosExecucao +
                          processo.ciclosStall;
    metricas.pcFinal = processo.contexto.PC;
    metricas.irFinal = processo.contexto.IR;
    for (size_t indice = 0; indice < processo.contexto.R.size(); ++indice) {
        metricas.registradores[indice] = processo.contexto.R[indice];
    }
    metricas.erroKernel = processo.erroKernel;
    return metricas;
}
}

ResultadoSimulacao Simulador::executarLote(
    const vector<string>& arquivosJobs, const Config& config) {
    ResultadoSimulacao resultado;
    Memoria memoria(TAM_RAM);
    MMUPaginada mmu(memoria, config.bitsDeslocamento, config.quadros,
                    config.substituicao);

    ClockCore::resetarSimulacao();
    {
        ofstream saida("output.dat", ios::trunc);
    }

    cout << "===================================================\n"
              << "[SIMULADOR RISC 16-BITS] - "
              << politicaParaTexto(config.politica) << " ("
              << config.numeroNucleos << " NUCLEO(S))\n"
              << "===================================================\n";

    for (size_t indice = 0; indice < arquivosJobs.size(); ++indice) {
        JobData job{};
        const int pid = static_cast<int>(indice) + 1;
        if (!JobParser::carregarArquivo(arquivosJobs[indice], job)) {
            cout << "[LOTE] Job " << pid << " ignorado.\n";
            continue;
        }

        const uint32_t tamanhoLogico = calcularTamanhoLogico(job);
        if (tamanhoLogico > numeric_limits<uint16_t>::max() ||
            tamanhoLogico == 0) {
            cout << "[LOTE] Job " << pid
                      << " excede o espaco de enderecamento logico e foi ignorado.\n";
            continue;
        }

        job.id = pid;
        vector<uint16_t> imagemLogica(tamanhoLogico, 0);
        for (size_t indice = 0; indice < job.instrucoes.size(); ++indice) {
            imagemLogica[indice] = job.instrucoes[indice];
        }
        for (const auto& variavel : job.variaveis) {
            imagemLogica[variavel.first] = variavel.second;
        }
        mmu.registrarProcesso(pid, imagemLogica);

        PCB processo(pid, job.nomeArquivo, job.prioridade, job.burst);
        processo.estado = EstadoProcesso::PRONTO;
        processo.enderecoBase = 0;
        processo.tamanhoMemoria = static_cast<uint16_t>(tamanhoLogico);
        processo.acessosDisco = 1;
        processo.ciclosCarga = 50;
        resultado.processos.push_back(processo);

        ClockCore::tickDisco(1);
        cout << "Job " << pid
                  << " carregado na RAM para a fila de prontos.\n";
    }

    resultado.ciclosPreparacao = ClockCore::getCycles();
    unique_ptr<Escalonador> escalonador =
        criarEscalonador(config.politica);
    vector<size_t> filaProntos;
    for (size_t indice = 0; indice < resultado.processos.size(); ++indice) {
        filaProntos.push_back(indice);
    }
    enum class EventoNucleo { OCIOSO, PRONTO_PARA_PASSO, PASSO, CONTEXTO };
    struct ExecucaoNucleo {
        unique_ptr<Nucleo> nucleo;
        EventoNucleo evento = EventoNucleo::OCIOSO;
        size_t processo = 0;
        uint64_t terminaEm = 0;
        uint32_t ciclosQuantum = 0;
        uint32_t acessosBarramentoRestantes = 0;
        uint32_t ciclosStallPosteriores = 0;
        int ultimoPid = -1;
        bool houveErro = false;
        bool passoTeveFaltaPagina = false;
        string mensagemErro;
    };
    vector<ExecucaoNucleo> nucleos(config.numeroNucleos);
    for (size_t indice = 0; indice < nucleos.size(); ++indice) {
        nucleos[indice].nucleo = make_unique<Nucleo>(
            static_cast<int>(indice), mmu);
    }

    uint64_t tempoAtual = 0;
    size_t proximoNucleoBarramento = 0;
    int nucleoDonoFaltaPagina = -1;
    size_t processosConcluidos = 0;
    const size_t quantidadeProcessos = resultado.processos.size();

    auto finalizarProcesso = [&](ExecucaoNucleo& execucao) {
        PCB& processo = resultado.processos[execucao.processo];
        if (nucleoDonoFaltaPagina == execucao.nucleo->getIdentificador()) {
            nucleoDonoFaltaPagina = -1;
        }
        const EstatisticasMMU estatisticas = mmu.obterEstatisticas(processo.pid);
        processo.hitsMemoria = estatisticas.hits;
        processo.faltasPagina = estatisticas.faltasPagina;
        processo.substituicoesPagina = estatisticas.substituicoesPagina;
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
            cout << "[INTERRUPCAO GERADA PELO HARDWARE] PID "
                      << processo.pid << ": " << execucao.mensagemErro << "\n";
        }
        Reporter::exibirTerminal(criarMetricas(processo));
        Reporter::salvarOutputDat("output.dat", criarMetricas(processo));
        imprimirResumoProcesso(processo);
        cout << "===================================================\n";
        ++processosConcluidos;
        execucao.nucleo->tornarOcioso();
        execucao.evento = EventoNucleo::OCIOSO;
        execucao.houveErro = false;
        execucao.mensagemErro.clear();
        execucao.ciclosQuantum = 0;
    };

    auto iniciarPasso = [&](ExecucaoNucleo& execucao) {
        PCB& processo = resultado.processos[execucao.processo];
        execucao.passoTeveFaltaPagina = false;
        try {
            if (processo.instrucoesExecutadas >=
                LIMITE_INSTRUCOES_POR_PROCESSO) {
                throw runtime_error(
                    "limite de instrucoes atingido sem HALT");
            }
            ResultadoPasso passo =
                execucao.nucleo->executarPasso(processo.pid);
            execucao.passoTeveFaltaPagina = passo.faltaPagina;
            if (passo.faltaPagina) {
                const int identificadorNucleo =
                    execucao.nucleo->getIdentificador();
                if (nucleoDonoFaltaPagina == -1) {
                    nucleoDonoFaltaPagina = identificadorNucleo;
                } else if (nucleoDonoFaltaPagina != identificadorNucleo) {
                    throw logic_error(
                        "Mais de um nucleo solicitou a MMU durante uma falta.");
                }
            }
            processo.acessosRAM += passo.acessosRAM;
            processo.ciclosExecucao += passo.acessosRAM;
            execucao.ciclosQuantum += passo.acessosRAM;
            processo.ciclosStall += passo.ciclosStall;
            if (passo.faltaPagina) {
                ++processo.acessosDisco;
            }
            if (passo.instrucaoExecutada) {
                ++processo.instrucoesExecutadas;
            } else if (passo.ciclosStall == 0) {
                throw runtime_error(
                    "CPU nao avancou nem concluiu a instrucao");
            }
            const uint32_t duracao = passo.acessosRAM + passo.ciclosStall;
            if (duracao == 0) {
                throw runtime_error("instrucao sem custo temporal");
            }
            execucao.evento = EventoNucleo::PASSO;
            execucao.ciclosStallPosteriores = passo.ciclosStall;
            if (config.contencaoBarramento) {
                execucao.acessosBarramentoRestantes = passo.acessosRAM;
                execucao.terminaEm = passo.acessosRAM == 0
                                         ? tempoAtual + passo.ciclosStall
                                         : numeric_limits<uint64_t>::max();
            } else {
                execucao.acessosBarramentoRestantes = 0;
                execucao.terminaEm = tempoAtual + duracao;
            }
        } catch (const exception& erro) {
            execucao.houveErro = true;
            execucao.mensagemErro = erro.what();
            finalizarProcesso(execucao);
        }
    };

    while (processosConcluidos < quantidadeProcessos) {
        // Despachos simultaneos seguem a ordem crescente dos nucleos.
        for (size_t indiceNucleo = 0; indiceNucleo < nucleos.size();
             ++indiceNucleo) {
            ExecucaoNucleo& execucao = nucleos[indiceNucleo];
            if (nucleoDonoFaltaPagina != -1 &&
                nucleoDonoFaltaPagina != static_cast<int>(indiceNucleo)) {
                continue;
            }
            if (execucao.evento != EventoNucleo::OCIOSO || filaProntos.empty()) {
                continue;
            }
            const size_t indice =
                escalonador->selecionarProximo(resultado.processos, filaProntos);
            filaProntos.erase(
                find(filaProntos.begin(), filaProntos.end(), indice));
            PCB& processo = resultado.processos[indice];
            execucao.processo = indice;
            execucao.ciclosQuantum = 0;
            execucao.houveErro = false;
            execucao.mensagemErro.clear();
            processo.estado = EstadoProcesso::EXECUTANDO;
            resultado.ordemExecucao.push_back(processo.pid);
            cout << "\nDespachando PID " << processo.pid << " no nucleo "
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
        for (size_t indiceNucleo = 0; indiceNucleo < nucleos.size();
             ++indiceNucleo) {
            ExecucaoNucleo& execucao = nucleos[indiceNucleo];
            if (nucleoDonoFaltaPagina != -1 &&
                nucleoDonoFaltaPagina != static_cast<int>(indiceNucleo)) {
                continue;
            }
            if (execucao.evento == EventoNucleo::PRONTO_PARA_PASSO) {
                iniciarPasso(execucao);
            }
        }

        uint64_t proximoEvento = numeric_limits<uint64_t>::max();
        bool haAcessoPendente = false;
        for (const ExecucaoNucleo& execucao : nucleos) {
            if (execucao.evento == EventoNucleo::PASSO &&
                execucao.acessosBarramentoRestantes > 0) {
                haAcessoPendente = true;
            }
            if ((execucao.evento == EventoNucleo::PASSO &&
                 execucao.acessosBarramentoRestantes == 0) ||
                execucao.evento == EventoNucleo::CONTEXTO) {
                proximoEvento = min(proximoEvento, execucao.terminaEm);
            }
        }
        const uint64_t instanteArbitragem = tempoAtual + 1;
        if (config.contencaoBarramento && haAcessoPendente) {
            proximoEvento = min(proximoEvento, instanteArbitragem);
        }
        if (proximoEvento == numeric_limits<uint64_t>::max()) {
            if (processosConcluidos == quantidadeProcessos) {
                break;
            }
            throw logic_error(
                "Simulacao sem eventos ativos e com processos pendentes.");
        }

        const uint64_t duracao = proximoEvento - tempoAtual;
        if (duracao > numeric_limits<uint32_t>::max()) {
            throw overflow_error("duracao da simulacao excedeu o limite");
        }
        if (nucleoDonoFaltaPagina != -1) {
            for (size_t indiceNucleo = 0; indiceNucleo < nucleos.size();
                 ++indiceNucleo) {
                ExecucaoNucleo& execucao = nucleos[indiceNucleo];
                if (static_cast<int>(indiceNucleo) == nucleoDonoFaltaPagina ||
                    execucao.evento != EventoNucleo::PRONTO_PARA_PASSO) {
                    continue;
                }
                PCB& processo = resultado.processos[execucao.processo];
                processo.ciclosStall += static_cast<uint32_t>(duracao);
                execucao.nucleo->registrarStallMMU(
                    static_cast<uint32_t>(duracao));
            }
        }
        ClockCore::avancarTempo(static_cast<uint32_t>(duracao));
        tempoAtual = proximoEvento;

        if (config.contencaoBarramento && haAcessoPendente &&
            tempoAtual == instanteArbitragem) {
            size_t nucleoAtendido = nucleos.size();
            for (size_t deslocamento = 0; deslocamento < nucleos.size();
                 ++deslocamento) {
                const size_t indice =
                    (proximoNucleoBarramento + deslocamento) % nucleos.size();
                if (nucleos[indice].evento == EventoNucleo::PASSO &&
                    nucleos[indice].acessosBarramentoRestantes > 0) {
                    nucleoAtendido = indice;
                    break;
                }
            }
            if (nucleoAtendido == nucleos.size()) {
                throw logic_error(
                    "Arbitragem sem solicitacao pendente no barramento.");
            }

            for (size_t indice = 0; indice < nucleos.size(); ++indice) {
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
            if (nucleoDonoFaltaPagina ==
                execucao.nucleo->getIdentificador() &&
                !execucao.passoTeveFaltaPagina) {
                nucleoDonoFaltaPagina = -1;
            }
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
    cout << "Preparacao do lote: " << resultado.ciclosPreparacao
              << " ciclos\n"
              << "Makespan do escalonamento: " << resultado.makespan
              << " ciclos\n"
              << "Trocas de contexto: " << resultado.ciclosContexto
              << " ciclos\n"
              << "Tempo total (preparacao + escalonamento): "
              << resultado.tempoTotal << " ciclos\n";

    return resultado;
}
