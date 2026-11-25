#include "ram_loader.hpp"
#include "../src/defines.hpp"

bool RAMLoader::carregarNaRAM(const JobData& job, Memoria& memoria,
                              uint16_t baseFisica) {
    if (baseFisica < OS_RESERVED_MEM || baseFisica >= memoria.getTamanho()) {
        return false;
    }

    const uint32_t limiteFisico = memoria.getTamanho();
    uint32_t enderecoInstrucao = baseFisica;

    for (uint16_t instrucao : job.instrucoes) {
        if (enderecoInstrucao >= limiteFisico) {
            return false;
        }
        memoria.escreverKernel(static_cast<uint16_t>(enderecoInstrucao), instrucao);
        enderecoInstrucao++;
    }

    for (const auto& variavel : job.variaveis) {
        uint32_t enderecoAbsoluto = static_cast<uint32_t>(baseFisica) + variavel.first;
        if (enderecoAbsoluto >= limiteFisico) {
            return false;
        }
        memoria.escreverKernel(static_cast<uint16_t>(enderecoAbsoluto), variavel.second);
    }

    return true;
}
