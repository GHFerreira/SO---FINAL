#include "../cpu.hpp"
#include "../../memoria.hpp"
#include "../../mmu.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "[FALHA] " << __FILE__ << ":" << __LINE__ \
                      << " Assercao falhou: " #condition << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void testCPUComULA() {
    Memoria memoria(TAM_RAM);
    MMUBaseLimite mmu(memoria);
    mmu.registrarProcesso(1, OS_RESERVED_MEM,
                          static_cast<uint16_t>(TAM_RAM - OS_RESERVED_MEM));
    CPU cpu(mmu);

    const std::vector<uint16_t> programa = {
        0x100A, 0x110B, 0x2201, 0x4320, 0x320C, 0xF000
    };
    for (size_t i = 0; i < programa.size(); ++i) {
        memoria.escreverKernel(OS_RESERVED_MEM + static_cast<uint16_t>(i), programa[i]);
    }
    memoria.escreverKernel(OS_RESERVED_MEM + 10, 15);
    memoria.escreverKernel(OS_RESERVED_MEM + 11, 25);

    for (int i = 0; i < 6; ++i) {
        ResultadoPasso resultado = cpu.step(1);
        TEST_ASSERT(resultado.instrucaoExecutada);
    }

    TEST_ASSERT(cpu.getRegisterBank().read(0) == 15);
    TEST_ASSERT(cpu.getRegisterBank().read(1) == 25);
    TEST_ASSERT(cpu.getRegisterBank().read(2) == 40);
    TEST_ASSERT(cpu.getRegisterBank().read(3) == 25);
    TEST_ASSERT(memoria.ler(OS_RESERVED_MEM + 12) == 40);
    TEST_ASSERT(cpu.isHalted());
    TEST_ASSERT(!cpu.getULA().overflow);
    std::cout << "Teste de integracao CPU + Memoria + ULA passou!\n";
}

int main() {
    testCPUComULA();
    return 0;
}
