#include "../cpu.hpp"
#include "../../memoria.hpp"
#include "../../mmu.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

int main() {
    Memoria memoria(TAM_RAM);
    MMUBaseLimite mmu(memoria);
    mmu.registrarProcesso(1, OS_RESERVED_MEM,
                          static_cast<uint16_t>(TAM_RAM - OS_RESERVED_MEM));

    ifstream arquivo("src/cpu/jobs/teste_condicional_beq.txt");
    if (!arquivo.is_open()) {
        cerr << "Erro ao abrir o programa de teste.\n";
        return 1;
    }

    string linha;
    uint16_t endereco = 0;
    while (getline(arquivo, linha)) {
        if (linha.empty()) continue;
        uint16_t dado = static_cast<uint16_t>(stoul(linha, nullptr, 16));
        memoria.escreverKernel(OS_RESERVED_MEM + endereco, dado);
        endereco++;
    }

    CPU cpu(mmu);
    uint32_t instrucoes = 0;
    while (!cpu.isHalted()) {
        ResultadoPasso resultado = cpu.step(1);
        instrucoes += resultado.instrucaoExecutada ? 1u : 0u;
    }

    cpu.imprimirEstado();
    cout << "Instrucoes executadas: " << instrucoes << '\n';
    cout << "Acessos a RAM: " << memoria.getTotalAcessos() << '\n';
    return 0;
}
