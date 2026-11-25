#include "../src/cpu/cpu.hpp"
#include "../src/memoria.hpp"
#include "../src/mmu.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void carregarPrograma(Memoria& memoria, uint16_t baseFisica,
                      uint16_t valorA, uint16_t valorB) {
    const std::vector<uint16_t> programa = {
        0x100A, 0x110B, 0x2201, 0x320C, 0xF000
    };
    for (size_t i = 0; i < programa.size(); ++i) {
        memoria.escreverKernel(baseFisica + static_cast<uint16_t>(i), programa[i]);
    }
    memoria.escreverKernel(baseFisica + 10, valorA);
    memoria.escreverKernel(baseFisica + 11, valorB);
}

void testarSoma(uint16_t valorA, uint16_t valorB) {
    Memoria memoria(TAM_RAM);
    MMUBaseLimite mmu(memoria);
    mmu.registrarProcesso(1, OS_RESERVED_MEM,
                          static_cast<uint16_t>(TAM_RAM - OS_RESERVED_MEM));
    carregarPrograma(memoria, OS_RESERVED_MEM, valorA, valorB);

    CPU cpu(mmu);
    uint32_t passos = 0;
    while (!cpu.isHalted()) {
        ResultadoPasso resultado = cpu.step(1);
        assert(resultado.instrucaoExecutada);
        assert(++passos < 100);
    }

    assert(memoria.ler(OS_RESERVED_MEM + 12) ==
           static_cast<uint16_t>(valorA + valorB));
}

int main() {
    testarSoma(5, 7);
    testarSoma(100, 23);
    std::cout << "Teste de integracao CPU-memoria passou!\n";
    return 0;
}
