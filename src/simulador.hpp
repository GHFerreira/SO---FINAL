#ifndef SIMULADOR_HPP
#define SIMULADOR_HPP

#include "config.hpp"
#include "pcb.hpp"
#include <cstdint>
#include <string>
#include <vector>

struct ResultadoSimulacao {
    std::vector<PCB> processos;
    uint32_t ciclosPreparacao = 0;
    uint32_t makespan = 0;
    uint32_t ciclosContexto = 0;
    uint32_t tempoTotal = 0;
};

class Simulador {
public:
    static ResultadoSimulacao executarLote(
        const std::vector<std::string>& arquivosJobs, const Config& config);
};

#endif // SIMULADOR_HPP
