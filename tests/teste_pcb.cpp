#include "src/pcb.hpp"
#include "loader/parser.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    std::cout << "--- Executando Testes de PCB e Diretivas do Parser ---" << std::endl;

    // 1. Inicializacao e utilitarios do PCB
    PCB pcb1(1, "job_alpha", 2, 15);
    assert(pcb1.pid == 1);
    assert(pcb1.nome == "job_alpha");
    assert(pcb1.prioridade == 2);
    assert(pcb1.burstEstimado == 15);
    assert(pcb1.tempoChegada == 0);
    assert(pcb1.estado == EstadoProcesso::NOVO);
    assert(!pcb1.halt);
    assert(!pcb1.erroKernel);
    assert(PCB::estadoParaTexto(EstadoProcesso::NOVO) == "NOVO");
    assert(PCB::estadoParaTexto(EstadoProcesso::PRONTO) == "PRONTO");
    assert(PCB::estadoParaTexto(EstadoProcesso::EXECUTANDO) == "EXECUTANDO");
    assert(PCB::estadoParaTexto(EstadoProcesso::BLOQUEADO) == "BLOQUEADO");
    assert(PCB::estadoParaTexto(EstadoProcesso::FINALIZADO) == "FINALIZADO");
    assert(PCB::estadoParaTexto(EstadoProcesso::ABORTADO) == "ABORTADO");

    pcb1.cicloFim = 42;
    assert(pcb1.calcularTempoRetorno() == 42);
    pcb1.instrucoesExecutadas = 10;
    pcb1.faltasPagina = 3;
    pcb1.resetarContadores();
    assert(pcb1.instrucoesExecutadas == 0);
    assert(pcb1.faltasPagina == 0);
    assert(pcb1.cicloFim == 0);
    assert(pcb1.estado == EstadoProcesso::NOVO);
    std::cout << "OK: inicializacao e reset de contadores do PCB validados" << std::endl;

    // 2. Salvamento e restauracao de contexto entre dois PCBs
    RegisterBank banco;
    for (size_t i = 0; i < RegisterBank::NUM_REGISTERS; ++i) {
        banco.write(i, static_cast<uint16_t>(0x1000 + i));
    }
    banco.setPC(0x0012);
    banco.setIR(0x2201);
    pcb1.salvarContexto(banco, false);

    PCB pcb2(2, "job_beta", 5, 30);
    for (size_t i = 0; i < RegisterBank::NUM_REGISTERS; ++i) {
        banco.write(i, static_cast<uint16_t>(0x2000 + i * 3));
    }
    banco.setPC(0x0044);
    banco.setIR(0xF000);
    pcb2.salvarContexto(banco, true);

    assert(!pcb1.halt);
    assert(pcb2.halt);

    pcb1.restaurarContexto(banco);
    for (size_t i = 0; i < RegisterBank::NUM_REGISTERS; ++i) {
        assert(banco.read(i) == static_cast<uint16_t>(0x1000 + i));
    }
    assert(banco.getPC() == 0x0012);
    assert(banco.getIR() == 0x2201);

    pcb2.restaurarContexto(banco);
    for (size_t i = 0; i < RegisterBank::NUM_REGISTERS; ++i) {
        assert(banco.read(i) == static_cast<uint16_t>(0x2000 + i * 3));
    }
    assert(banco.getPC() == 0x0044);
    assert(banco.getIR() == 0xF000);
    std::cout << "OK: troca de contexto entre PCBs validada" << std::endl;

    // 3. Compatibilidade retroativa com jobs antigos (sem diretivas)
    JobData jobAntigo;
    bool carregouAntigo = JobParser::carregarArquivo("tests/job1_soma.txt", jobAntigo);
    assert(carregouAntigo);
    assert(jobAntigo.prioridade == 0);
    assert(jobAntigo.instrucoes.size() == 5);
    assert(jobAntigo.burst == 5);
    std::cout << "OK: compatibilidade do JobParser com jobs antigos validada" << std::endl;

    // 4. Leitura das diretivas .priority e .burst
    const std::string caminhoTemporario = "build/teste_job_diretivas.txt";
    {
        std::ofstream arquivo(caminhoTemporario);
        assert(arquivo.is_open());
        arquivo << "// Job de teste com diretivas\n";
        arquivo << ".priority 3\n";
        arquivo << ".burst 25\n";
        arquivo << "100A\n";
        arquivo << "F000\n";
        arquivo << ".data\n";
        arquivo << "0A 0007\n";
    }

    JobData jobComDiretivas;
    bool carregouNovo = JobParser::carregarArquivo(caminhoTemporario, jobComDiretivas);
    assert(carregouNovo);
    assert(jobComDiretivas.prioridade == 3);
    assert(jobComDiretivas.burst == 25);
    assert(jobComDiretivas.instrucoes.size() == 2);
    assert(jobComDiretivas.variaveis.size() == 1);
    std::cout << "OK: leitura das diretivas .priority e .burst validada" << std::endl;

    std::cout << "Todos os testes de PCB e Parser passaram!" << std::endl;
    return 0;
}
