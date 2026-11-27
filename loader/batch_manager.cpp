#include "batch_manager.hpp"

#include "clock_types.hpp"
#include "clock_core.hpp"
#include "metrics.hpp"
#include "parser.hpp"
#include "ram_loader.hpp"
#include "reporter.hpp"

#include "../src/cpu/cpu.hpp"
#include "../src/defines.hpp"
#include "../src/memoria.hpp"
#include "../src/mmu.hpp"

#include <cstddef>
#include <exception>
#include <iostream>

using namespace std;

/**
 * BatchManager - Gerenciador do ciclo de vida em lote
 *
 * Percorre a fila de jobs executando cada um de forma isolada. A cada job o
 * contexto da máquina é reiniciado (PC lógico = 0, registradores zerados, clock do
 * job zerado), de modo que nenhum resíduo do job anterior influencie o próximo.
 *
 * Princípio do processamento em lote: a falha de um job não derruba o lote.
 * Erros de leitura, de carga na RAM ou interrupções de hardware abortam apenas
 * o job corrente, e a execução prossegue para o próximo da fila.
 */

namespace {

/** Arquivo de saída persistente exigido pela especificação */
const char* ARQUIVO_SAIDA = "output.dat";

/**
 * Limite de instruções por job (proteção do lote).
 * Um job cujo fluxo nunca alcance o HALT (ex.: laço JUMP mal formado) travaria
 * o simulador indefinidamente. Ao atingir o teto o job é abortado e o lote segue.
 */
const uint32_t LIMITE_INSTRUCOES_POR_JOB = 1000000;

} // namespace

void BatchManager::executarLote(const vector<string>& listaArquivosJobs) {
    Memoria memoria(TAM_RAM);
    MMUBaseLimite mmu(memoria);

    cout << "===================================================" << endl;
    cout << "[SIMULADOR RISC 16-BITS] - EXECUCAO EM LOTE" << endl;
    cout << "===================================================" << endl;

    for (size_t i = 0; i < listaArquivosJobs.size(); ++i) {
        const string& caminho = listaArquivosJobs[i];
        const int idJob = static_cast<int>(i) + 1;

        // --- 1. Leitura do job em disco (JobParser) ------------------------
        JobData job;
        if (!JobParser::carregarArquivo(caminho, job)) {
            cout << "[LOTE] Falha ao ler o job " << idJob << ": " << caminho
                      << " (job ignorado)" << endl;
            continue;
        }

        job.id = idJob;
        if (job.nomeArquivo.empty()) {
            job.nomeArquivo = caminho;
        }

        cout << "Carregando Job " << job.id << ": " << job.nomeArquivo << "..." << endl;

        // --- 2. Reset de contexto e Telemetria de Disco -------------------
        // Zera o clock do job, abre a telemetria e devolve a CPU ao estado
        // inicial. O PC aponta para o início lógico do job.
        ClockCore::reset();
        MetricsTracker::iniciarJob(job.id, job.nomeArquivo);
        memoria.limpar();
        mmu.limparProcessos();
        mmu.registrarProcesso(job.id, OS_RESERVED_MEM,
                              static_cast<uint16_t>(TAM_RAM - OS_RESERVED_MEM));
        CPU cpu(mmu);
        cpu.reset();
        cpu.setPC(0);

        // O carregamento do Job a partir do Disco Virtual para a RAM gera 1 acesso
        // a disco (I/O) com penalidade inicial de 50 ciclos de relógio
        ClockCore::tickDisco(1);
        MetricsTracker::registrarAcessoDisco();

        // --- 3. Carga das instruções e variáveis na RAM (RAMLoader) --------
        const bool cargaConcluida = RAMLoader::carregarNaRAM(job, memoria);
        if (!cargaConcluida) {
            cout << "[LOTE] Falha ao carregar o job " << job.id
                      << " na RAM (job abortado)" << endl;
            MetricsTracker::marcarErroKernel();
        }

        // --- 4. Execução do job -------------------------------------------
        if (cargaConcluida) {
            uint32_t instrucoesDoJob = 0;
            try {
                while (!cpu.isHalted()) {
                    if (instrucoesDoJob >= LIMITE_INSTRUCOES_POR_JOB) {
                        cout << "[LOTE] Job " << job.id
                                  << " excedeu o limite de instrucoes sem HALT (job abortado)"
                                  << endl;
                        MetricsTracker::marcarErroKernel();
                        break;
                    }

                    ResultadoPasso resultado = cpu.step(job.id);

                    if (resultado.instrucaoExecutada) {
                        MetricsTracker::registrarInstrucao();
                        ++instrucoesDoJob;
                    }
                    if (resultado.ciclosStall > 0) {
                        ClockCore::adicionarCiclos(resultado.ciclosStall);
                    }
                }

                if (cpu.isHalted()) {
                    cout << "Execucao finalizada (HALT encontrado no PC 0x"
                              << hex << uppercase << cpu.getRegisterBank().getPC() << dec << ")." << endl;
                }
            } catch (const exception& e) {
                // Interrupção de hardware (SEGFAULT na área do SO ou opcode inválido).
                // Encerra somente este job e preserva as métricas coletadas até aqui.
                cout << "[INTERRUPCAO GERADA PELO HARDWARE]: " << e.what() << endl;
                MetricsTracker::marcarErroKernel();
            }

            const uint32_t acessosRAM = memoria.getTotalAcessos();
            if (acessosRAM > 0) {
                MetricsTracker::registrarAcessoRAM(acessosRAM);
                for (uint32_t a = 0; a < acessosRAM; ++a) {
                    ClockCore::tickRAM();
                }
            }
        }

        // --- 5. Relatório do job ------------------------------------------
        // Captura o snapshot real da CPU (PC, IR e Registradores R0..R7).
        // A CPU modular expõe o estado pelo RegisterBank (getPC/getIR/snapshot),
        // então lemos os registradores por ele no lugar dos getters antigos.
        const RegisterBank& regBank = cpu.getRegisterBank();
        const vector<uint16_t> registradores = regBank.getRegistersSnapshot();
        MetricsTracker::capturarEstadoCPU(regBank.getPC(), regBank.getIR(),
                                          registradores.data());

        const JobMetrics metricas = MetricsTracker::obterMetricas();
        Reporter::exibirTerminal(metricas);
        Reporter::salvarOutputDat(ARQUIVO_SAIDA, metricas);

        cout << "===================================================" << endl;
    }
}
