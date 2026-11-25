#ifndef RAM_LOADER_HPP
#define RAM_LOADER_HPP

#include "clock_types.hpp"
#include "../src/defines.hpp"
#include "../src/memoria.hpp"
#include <cstdint>

/**
 * RAMLoader - Mapeamento e alocação de instruções e dados na RAM física (a partir de 512)
 * Integrante responsável: Integrante 6
 */
class RAMLoader {
public:
    static bool carregarNaRAM(const JobData& job, Memoria& memoria,
                              uint16_t baseFisica = OS_RESERVED_MEM);
};

#endif // RAM_LOADER_HPP
