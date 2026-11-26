#ifndef ESCALONADOR_HPP
#define ESCALONADOR_HPP

#include "config.hpp"
#include "pcb.hpp"
#include <cstddef>
#include <memory>
#include <vector>

class Escalonador {
public:
    virtual ~Escalonador() = default;
    virtual std::size_t selecionarProximo(
        const std::vector<PCB>& processos,
        const std::vector<std::size_t>& filaProntos) const = 0;
};

std::unique_ptr<Escalonador> criarEscalonador(PoliticaEscalonamento politica);

#endif // ESCALONADOR_HPP
