#include "src/memoria.hpp"
#include "src/mmu.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    std::cout << "--- Executando Testes de Memoria e MMUBaseLimite ---" << std::endl;

    // 1. Testes da classe Memoria
    Memoria memoria;
    assert(memoria.getTamanho() == TAM_RAM);
    assert(memoria.getTotalAcessos() == 0);

    memoria.escrever(OS_RESERVED_MEM, 0x1234);
    assert(memoria.ler(OS_RESERVED_MEM) == 0x1234);
    assert(memoria.getTotalAcessos() == 2);

    memoria.escrever(static_cast<uint16_t>(TAM_RAM - 1), 0xABCD);
    assert(memoria.ler(static_cast<uint16_t>(TAM_RAM - 1)) == 0xABCD);
    assert(memoria.getTotalAcessos() == 4);

    // Leitura/escrita de kernel nao incrementa acessos e pode acessar area do SO
    memoria.escreverKernel(0x0010, 0xCAFE);
    assert(memoria.lerKernel(0x0010) == 0xCAFE);
    assert(memoria.getTotalAcessos() == 4);

    // Protecao contra acesso de usuario na area reservada do SO
    bool falhouLeituraSO = false;
    try {
        memoria.ler(OS_RESERVED_MEM - 1);
    } catch (const std::runtime_error&) {
        falhouLeituraSO = true;
    }
    assert(falhouLeituraSO);

    bool falhouEscritaSO = false;
    try {
        memoria.escrever(0x0000, 0x0001);
    } catch (const std::runtime_error&) {
        falhouEscritaSO = true;
    }
    assert(falhouEscritaSO);

    memoria.limpar();
    assert(memoria.getTotalAcessos() == 0);
    assert(memoria.lerKernel(0x0010) == 0);
    assert(memoria.ler(OS_RESERVED_MEM) == 0);
    std::cout << "OK: classe Memoria validada" << std::endl;

    // 2. Testes da MMUBaseLimite com dois processos
    memoria.limpar();
    MMUBaseLimite mmu(memoria);
    mmu.registrarProcesso(1, 512, 64);
    mmu.registrarProcesso(2, 576, 64);
    assert(mmu.possuiProcesso(1));
    assert(mmu.possuiProcesso(2));

    ResultadoAcesso res1 = mmu.traduzir(1, 0x0000);
    assert(res1.enderecoFisico == 512);
    assert(!res1.faltaPagina);
    assert(res1.ciclosStall == 0);

    ResultadoAcesso res1Offset = mmu.traduzir(1, 0x000A);
    assert(res1Offset.enderecoFisico == 522);

    ResultadoAcesso res2 = mmu.traduzir(2, 0x0000);
    assert(res2.enderecoFisico == 576);
    ResultadoAcesso res2Offset = mmu.traduzir(2, 0x000A);
    assert(res2Offset.enderecoFisico == 586);

    // Isolamento de escrita/leitura no mesmo endereco logico (0x000A)
    ResultadoAcesso acessoEscrita;
    mmu.escrever(1, 0x000A, 0x0055, &acessoEscrita);
    assert(acessoEscrita.enderecoFisico == 522);
    mmu.escrever(2, 0x000A, 0x0099);

    ResultadoAcesso acessoLeitura;
    assert(mmu.ler(1, 0x000A, &acessoLeitura) == 0x0055);
    assert(acessoLeitura.enderecoFisico == 522);
    assert(mmu.ler(2, 0x000A) == 0x0099);
    std::cout << "OK: traducao e isolamento entre processos validados" << std::endl;

    // 3. Protecoes de limite e processo inexistente
    bool falhouLimiteExato = false;
    try {
        mmu.traduzir(1, 64);
    } catch (const std::runtime_error&) {
        falhouLimiteExato = true;
    }
    assert(falhouLimiteExato);

    bool falhouAcimaLimite = false;
    try {
        mmu.ler(1, 100);
    } catch (const std::runtime_error&) {
        falhouAcimaLimite = true;
    }
    assert(falhouAcimaLimite);

    bool falhouPidInexistente = false;
    try {
        mmu.traduzir(99, 0);
    } catch (const std::runtime_error&) {
        falhouPidInexistente = true;
    }
    assert(falhouPidInexistente);

    // Validacao de parametros de registro
    bool falhouBaseSO = false;
    try {
        mmu.registrarProcesso(3, 500, 32);
    } catch (const std::invalid_argument&) {
        falhouBaseSO = true;
    }
    assert(falhouBaseSO);

    bool falhouLimiteZero = false;
    try {
        mmu.registrarProcesso(3, 700, 0);
    } catch (const std::invalid_argument&) {
        falhouLimiteZero = true;
    }
    assert(falhouLimiteZero);

    bool falhouEstouroRAM = false;
    try {
        mmu.registrarProcesso(3, 65500, 100);
    } catch (const std::invalid_argument&) {
        falhouEstouroRAM = true;
    }
    assert(falhouEstouroRAM);

    // Remocao e limpeza de processos
    mmu.removerProcesso(1);
    assert(!mmu.possuiProcesso(1));
    assert(mmu.possuiProcesso(2));
    mmu.limparProcessos();
    assert(!mmu.possuiProcesso(2));
    std::cout << "OK: protecoes e ciclo de vida da MMU validados" << std::endl;

    std::cout << "Todos os testes de Memoria e MMU passaram!" << std::endl;
    return 0;
}
