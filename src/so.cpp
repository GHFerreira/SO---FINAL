#include "so.hpp"
#include "../loader/clock_core.hpp"
#include <iostream>
#include <iomanip>
#include <exception>

SistemaOperacional::SistemaOperacional()
    : memoria(TAM_RAM), mmu(memoria), cpu(mmu) {}


void SistemaOperacional::carregarJob(const std::vector<uint16_t>& jobInstrucoes,
                                     const std::vector<uint16_t>& dadosIniciais) {
    const uint16_t baseMemoria = OS_RESERVED_MEM;
    mmu.limparProcessos();
    mmu.registrarProcesso(1, baseMemoria,
                          static_cast<uint16_t>(memoria.getTamanho() - baseMemoria));
    memoria.limpar();

    // Carrega as instruções do job na RAM (a partir do endereço base)
    for (size_t i = 0; i < jobInstrucoes.size(); i++) {
        memoria.escreverKernel(static_cast<uint16_t>(baseMemoria + i), jobInstrucoes[i]);
    }

    // Simula a carga de variáveis nas posições 10, 11 e 12 (relativas ao Job)
    // A = dadosIniciais[0], B = dadosIniciais[1], C = 0 (inicialmente)
    memoria.escreverKernel(baseMemoria + 10, dadosIniciais[0]);
    memoria.escreverKernel(baseMemoria + 11, dadosIniciais[1]);
    memoria.escreverKernel(baseMemoria + 12, 0);

    cpu.setPC(0);
}

void SistemaOperacional::executarJob(int idJob,
                                     const std::string& nomeJob,
                                     const std::vector<uint16_t>& inst,
                                     const std::vector<uint16_t>& dados) {
    std::cout << "\n===================================================" << std::endl;
    std::cout << "[SIMULADOR RISC 16-BITS] - EXECUCAO EM LOTE" << std::endl;
    std::cout << "===================================================" << std::endl;
    std::cout << "Carregando Job " << idJob << ": " << nomeJob << "..." << std::endl;

    ClockCore::reset();
    cpu.reset();

    // Loader: copia o job para a RAM
    carregarJob(inst, dados);

    // Laço de execução bare-metal (sequencial)
    uint32_t instrucoesExecutadas = 0;
    uint32_t acessosRAMContabilizados = 0;
    const uint32_t acessosDisco = 0;
    auto contabilizarAcessosRAM = [&]() {
        const uint32_t totalAcessos = memoria.getTotalAcessos();
        while (acessosRAMContabilizados < totalAcessos) {
            ClockCore::tickRAM();
            ++acessosRAMContabilizados;
        }
    };

    try {
        while (!cpu.isHalted()) {
            ResultadoPasso resultado = cpu.step(1);
            if (resultado.instrucaoExecutada) {
                ++instrucoesExecutadas;
            }
            contabilizarAcessosRAM();
            if (resultado.ciclosStall > 0) {
                ClockCore::adicionarCiclos(resultado.ciclosStall);
            }
        }
        std::cout << "Execucao finalizada (HALT encontrado)." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "\n[ERRO CRITICO] " << e.what() << std::endl;
        std::cout << "O Job " << idJob << " foi abortado pelo Sistema Operacional." << std::endl;
    }
    contabilizarAcessosRAM();

    // Exibe o estado final da CPU
    cpu.imprimirEstado();

    // Exibe as estatísticas do job
    std::cout << "--- ESTATISTICAS DO JOB ---" << std::endl;
    std::cout << "Instrucoes Executadas: " << instrucoesExecutadas << std::endl;
    std::cout << "Acessos a Memoria (RAM): " << memoria.getTotalAcessos() << std::endl;
    std::cout << "Acessos ao Disco (I/O): " << acessosDisco << std::endl;
    std::cout << "CLOCK TOTAL DESTE JOB: " << ClockCore::getCycles() << " ciclos" << std::endl;
    std::cout << "===================================================\n" << std::endl;
}
