#include "nucleo.hpp"

using namespace std;

Nucleo::Nucleo(int identificador, MMU& mmu)
    : identificador(identificador), cpu(mmu), estado(EstadoNucleo::OCIOSO) {}

ResultadoPasso Nucleo::executarPasso(int pid) {
    estado = EstadoNucleo::EXECUTANDO;
    ResultadoPasso resultado = cpu.step(pid);

    contadores.ciclosExecucao += resultado.acessosRAM;
    contadores.ciclosStall += resultado.ciclosStall;
    if (resultado.instrucaoExecutada) {
        ++contadores.instrucoesExecutadas;
    }

    if (resultado.faltaPagina) {
        estado = EstadoNucleo::STALL_FALTA_PAGINA;
    } else if (cpu.isHalted()) {
        estado = EstadoNucleo::OCIOSO;
    }

    return resultado;
}

void Nucleo::iniciarTrocaContexto(uint32_t ciclos) {
    estado = EstadoNucleo::TROCA_CONTEXTO;
    contadores.ciclosContexto += ciclos;
}

void Nucleo::registrarStallBarramento(uint32_t ciclos) {
    contadores.ciclosStall += ciclos;
    contadores.ciclosContencaoBarramento += ciclos;
}

void Nucleo::registrarStallMMU(uint32_t ciclos) {
    contadores.ciclosStall += ciclos;
}

void Nucleo::retomarExecucao() {
    estado = EstadoNucleo::EXECUTANDO;
}

void Nucleo::registrarOciosidade(uint32_t ciclos) {
    estado = EstadoNucleo::OCIOSO;
    contadores.ciclosOciosos += ciclos;
}

void Nucleo::tornarOcioso() {
    estado = EstadoNucleo::OCIOSO;
}

void Nucleo::reiniciar() {
    cpu.reset();
    estado = EstadoNucleo::OCIOSO;
    contadores = ContadoresNucleo{};
}

CPUContext Nucleo::salvarContexto() const {
    return cpu.getRegisterBank().getContext();
}

void Nucleo::restaurarContexto(const CPUContext& contexto) {
    cpu.reset();
    cpu.getRegisterBank().setContext(contexto);
}

bool Nucleo::processoFinalizado() const {
    return cpu.isHalted();
}

int Nucleo::getIdentificador() const {
    return identificador;
}

EstadoNucleo Nucleo::getEstado() const {
    return estado;
}

const ContadoresNucleo& Nucleo::getContadores() const {
    return contadores;
}

string Nucleo::estadoParaTexto(EstadoNucleo estado) {
    switch (estado) {
        case EstadoNucleo::OCIOSO:
            return "OCIOSO";
        case EstadoNucleo::EXECUTANDO:
            return "EXECUTANDO";
        case EstadoNucleo::TROCA_CONTEXTO:
            return "TROCA_CONTEXTO";
        case EstadoNucleo::STALL_FALTA_PAGINA:
            return "STALL_FALTA_PAGINA";
    }
    return "DESCONHECIDO";
}
