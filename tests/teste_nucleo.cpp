#include "../src/nucleo.hpp"
#include "../src/defines.hpp"
#include "../src/memoria.hpp"
#include <cassert>
#include <iostream>

class MMUComFaltaInicial : public MMU {
private:
    Memoria& memoria;
    bool primeiraLeitura = true;

public:
    explicit MMUComFaltaInicial(Memoria& memoria) : memoria(memoria) {}

    ResultadoAcesso traduzir(int, uint16_t enderecoLogico) override {
        ResultadoAcesso resultado;
        if (primeiraLeitura) {
            primeiraLeitura = false;
            resultado.faltaPagina = true;
            resultado.ciclosStall = 6;
            return resultado;
        }
        resultado.enderecoFisico = OS_RESERVED_MEM + enderecoLogico;
        return resultado;
    }

    uint16_t ler(int pid, uint16_t enderecoLogico,
                 ResultadoAcesso* resultado = nullptr) override {
        ResultadoAcesso acesso = traduzir(pid, enderecoLogico);
        if (resultado != nullptr) *resultado = acesso;
        if (acesso.faltaPagina) return 0;
        return memoria.ler(acesso.enderecoFisico);
    }

    void escrever(int pid, uint16_t enderecoLogico, uint16_t dado,
                  ResultadoAcesso* resultado = nullptr) override {
        ResultadoAcesso acesso = traduzir(pid, enderecoLogico);
        if (resultado != nullptr) *resultado = acesso;
        if (!acesso.faltaPagina) memoria.escrever(acesso.enderecoFisico, dado);
    }
};

void testarEstadosECiclos() {
    Memoria memoria(TAM_RAM);
    MMUComFaltaInicial mmu(memoria);
    memoria.escreverKernel(OS_RESERVED_MEM, 0xF000);
    Nucleo nucleo(0, mmu);

    assert(nucleo.getEstado() == EstadoNucleo::OCIOSO);
    nucleo.iniciarTrocaContexto(3);
    assert(nucleo.getEstado() == EstadoNucleo::TROCA_CONTEXTO);

    ResultadoPasso resultado = nucleo.executarPasso(7);
    assert(resultado.faltaPagina);
    assert(resultado.ciclosStall == 6);
    assert(nucleo.getEstado() == EstadoNucleo::STALL_FALTA_PAGINA);
    assert(nucleo.getContadores().ciclosStall == 6);

    nucleo.retomarExecucao();
    resultado = nucleo.executarPasso(7);
    assert(resultado.instrucaoExecutada);
    assert(resultado.acessosRAM == 1);
    assert(nucleo.processoFinalizado());
    assert(nucleo.getEstado() == EstadoNucleo::OCIOSO);
    assert(nucleo.getContadores().ciclosExecucao == 1);
    assert(nucleo.getContadores().ciclosContexto == 3);
    assert(nucleo.getContadores().instrucoesExecutadas == 1);

    nucleo.registrarStallBarramento(2);
    assert(nucleo.getContadores().ciclosStall == 8);
    assert(nucleo.getContadores().ciclosContencaoBarramento == 2);

    nucleo.registrarOciosidade(4);
    assert(nucleo.getContadores().ciclosOciosos == 4);
}

void testarCapturaERestauracaoContexto() {
    Memoria memoria(TAM_RAM);
    MMUBaseLimite mmu(memoria);
    mmu.registrarProcesso(1, OS_RESERVED_MEM, 20);
    Nucleo nucleo(2, mmu);
    CPUContext contexto{};
    contexto.R.fill(0);
    contexto.R[3] = 42;
    contexto.PC = 5;
    contexto.IR = 0x1234;

    nucleo.restaurarContexto(contexto);
    CPUContext salvo = nucleo.salvarContexto();
    assert(salvo.R[3] == 42);
    assert(salvo.PC == 5);
    assert(salvo.IR == 0x1234);
    assert(nucleo.getIdentificador() == 2);
}

int main() {
    testarEstadosECiclos();
    testarCapturaERestauracaoContexto();
    std::cout << "Todos os testes de Nucleo passaram!\n";
    return 0;
}
