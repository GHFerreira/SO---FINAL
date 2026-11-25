#include "../src/memoria.hpp"
#include "../loader/clock_core.hpp"
#include "../loader/metrics.hpp"
#include "../loader/parser.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void testeLeituraEscrita() {
    Memoria memoria(TAM_RAM);
    memoria.escrever(600, 42);
    assert(memoria.ler(600) == 42);
    assert(memoria.getTotalAcessos() == 2);
    std::cout << "OK: leitura, escrita e contagem de acessos\n";
}

void testeMemoriaNaoAlteraRelogio() {
    Memoria memoria(TAM_RAM);
    ClockCore::reset();
    memoria.escrever(600, 1);
    memoria.ler(600);
    assert(ClockCore::getCycles() == 0);
    ClockCore::tickRAM();
    ClockCore::tickRAM();
    assert(ClockCore::getCycles() == 2);
    std::cout << "OK: relogio unico contabiliza os ciclos de RAM\n";
}

void testeProtecaoLeitura() {
    Memoria memoria(TAM_RAM);
    bool lancou = false;
    try {
        memoria.ler(0);
    } catch (const std::exception&) {
        lancou = true;
    }
    assert(lancou);
    std::cout << "OK: protecao de leitura na area do SO\n";
}

void testeProtecaoEscrita() {
    Memoria memoria(TAM_RAM);
    bool lancou = false;
    try {
        memoria.escrever(511, 99);
    } catch (const std::exception&) {
        lancou = true;
    }
    assert(lancou);
    std::cout << "OK: protecao de escrita na area do SO\n";
}

void testeEnderecoMaximo() {
    Memoria memoria(TAM_RAM);
    memoria.escrever(65535, 123);
    assert(memoria.ler(65535) == 123);
    std::cout << "OK: endereco maximo (65535) funciona\n";
}

void testeLeituraDisco() {
    const std::vector<uint16_t> disco = {10, 20, 30, 999};
    ClockCore::reset();
    MetricsTracker::iniciarJob(1, "teste_disco");
    int ciclos = JobParser::lerDisco(30, disco);
    assert(ciclos == 150);
    assert(ClockCore::getCycles() == 150);
    assert(MetricsTracker::obterMetricas().acessosDisco == 1);
    std::cout << "OK: custo e telemetria do acesso a disco\n";
}

void testeLeituraDiscoSemEncontrar() {
    const std::vector<uint16_t> disco = {10, 20, 30};
    ClockCore::reset();
    MetricsTracker::iniciarJob(2, "teste_disco_sem_resultado");
    int ciclos = JobParser::lerDisco(999, disco);
    assert(ciclos == 150);
    assert(ClockCore::getCycles() == 150);
    assert(MetricsTracker::obterMetricas().acessosDisco == 1);
    std::cout << "OK: custo do disco quando o dado nao e encontrado\n";
}

int main() {
    testeLeituraEscrita();
    testeMemoriaNaoAlteraRelogio();
    testeProtecaoLeitura();
    testeProtecaoEscrita();
    testeEnderecoMaximo();
    testeLeituraDisco();
    testeLeituraDiscoSemEncontrar();
    std::cout << "Todos os testes de memoria passaram!\n";
    return 0;
}
