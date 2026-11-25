#include "control_unit.hpp"
#include <stdexcept>
#include <string>

namespace {
struct FaltaPagina {};
}

// ----------------------------------------------------------------------------
// Construtor e Controle Básico
// ----------------------------------------------------------------------------
ControlUnit::ControlUnit(MMU &mmu, int pid) : mmu(mmu), pid(pid), haltStatus(false) {}

void ControlUnit::reset() { haltStatus = false; }

bool ControlUnit::isHalted() const { return haltStatus; }

// ----------------------------------------------------------------------------
// Etapa 1: BUSCA (Fetch)
// ----------------------------------------------------------------------------
uint16_t ControlUnit::lerMemoria(uint16_t enderecoLogico,
                                 ResultadoPasso &resultado) {
  ResultadoAcesso acesso;
  uint16_t dado = mmu.ler(pid, enderecoLogico, &acesso);
  if (acesso.faltaPagina) {
    resultado.faltaPagina = true;
    resultado.ciclosStall += acesso.ciclosStall;
    throw FaltaPagina{};
  }
  return dado;
}

void ControlUnit::escreverMemoria(uint16_t enderecoLogico, uint16_t dado,
                                  ResultadoPasso &resultado) {
  ResultadoAcesso acesso;
  mmu.escrever(pid, enderecoLogico, dado, &acesso);
  if (acesso.faltaPagina) {
    resultado.faltaPagina = true;
    resultado.ciclosStall += acesso.ciclosStall;
    throw FaltaPagina{};
  }
}

uint16_t ControlUnit::buscar(RegisterBank &regBank, ResultadoPasso &resultado) {
  uint16_t pc = regBank.getPC();
  uint16_t IR = lerMemoria(pc, resultado);

  regBank.setIR(IR);
  regBank.incrementPC(); // Avança o PC para a próxima palavra

  return IR;
}

// ----------------------------------------------------------------------------
// Etapa 2: DECODIFICAÇÃO (Decode)
// ----------------------------------------------------------------------------
InstrucaoDecodificada ControlUnit::decode(uint16_t IR) const {
  InstrucaoDecodificada instr;

  instr.opcode = (IR >> 12) & 0x000F; // Bits [15:12] -> Código da Operação
  instr.regDest =
      (IR >> 8) & 0x000F; // Bits [11:8]  -> Registrador de Destino (ou Fonte 1)
  instr.regF1 =
      (IR >> 4) & 0x000F;       // Bits [7:4]   -> Primeiro Registrador Fonte
  instr.regF2 = IR & 0x000F;    // Bits [3:0]   -> Segundo Registrador Fonte
  instr.endereco = IR & 0x00FF; // Bits [7:0]   -> Endereço imediato (Tipo I)
  instr.enderecoJump = IR & 0x0FFF; // Bits [11:0]  -> Endereço absoluto (JUMP)

  return instr;
}

// ----------------------------------------------------------------------------
// Despacho para a ULA (operações puramente computacionais)
// ----------------------------------------------------------------------------
void ControlUnit::dispararULA(RegisterBank &regBank, ULA &ula, uint16_t opcode,
                              uint16_t regDest, uint16_t regF1,
                              uint16_t regF2) {
  // 1. A UC lê os operandos do RegisterBank...
  uint16_t val1 = regBank.read(regF1);
  uint16_t val2 = regBank.read(regF2);

  // 2. ...e comanda a ULA para realizar o cálculo (a UC não calcula nada).
  if (!ula.executeOpcode(opcode, val1, val2)) {
    throw std::runtime_error(
        "ControlUnit::dispararULA - Opcode invalido ou nao computacional: " +
        std::to_string(opcode));
  }

  // 3. A UC grava o resultado devolvido pela ULA de volta no destino.
  regBank.write(regDest, static_cast<uint16_t>(ula.result));
}

// ----------------------------------------------------------------------------
// Etapa 3: EXECUÇÃO (Execute)
// ----------------------------------------------------------------------------
void ControlUnit::executar(const InstrucaoDecodificada &instr,
                           RegisterBank &regBank, ULA &ula,
                           ResultadoPasso &resultado) {
  switch (instr.opcode) {

  // --- Transferência de Memória (Paradigma Load/Store) ---
  case OP_LOAD:
    regBank.write(instr.regDest, lerMemoria(instr.endereco, resultado));
    break;

  case OP_STORE:
    escreverMemoria(instr.endereco, regBank.read(instr.regDest), resultado);
    break;

  // --- Aritmética e Lógica (delegadas integralmente à ULA) ---
  case OP_ADD:
  case OP_SUB:
  case OP_AND:
  case OP_OR:
    dispararULA(regBank, ula, instr.opcode, instr.regDest, instr.regF1,
                instr.regF2);
    break;

  // --- Controle de Fluxo ---
  case OP_BEQ:
    // A UC pede à ULA para comparar (via subtração); a UC decide o desvio.
    // Obs: BEQ não é uma operação da ULA (enum operation só tem
    // ADD/SUB/AND_OP/OR_OP) — o desvio é lido a partir do resultado
    // de uma subtração: se (regDest - regF1) == 0, os valores são iguais.
    ula.execute(SUB, regBank.read(instr.regDest), regBank.read(instr.regF1));
    if (ula.result == 0) {
      regBank.setPC(instr.endereco);
    }
    break;

  case OP_JUMP:
    // Salto incondicional: a UC altera o PC diretamente (não é
    // responsabilidade da ULA, conforme diretrizes do projeto).
    regBank.setPC(instr.enderecoJump);
    break;

  // --- Sistema ---
  case OP_HALT:
    // Fim do job atual: sinaliza para o Sistema Operacional (Loader)
    // encerrar o laço de execução e carregar o próximo job do lote.
    haltStatus = true;
    break;

  default:
    throw std::runtime_error(
        "ControlUnit::execute - Opcode Invalido detectado: 0x" +
        std::to_string(instr.opcode));
  }
}

// ----------------------------------------------------------------------------
// Ciclo Completo de Instrução (Fetch -> Decode -> Execute)
// ----------------------------------------------------------------------------
ResultadoPasso ControlUnit::step(RegisterBank &regBank, ULA &ula) {
  ResultadoPasso resultado;
  if (haltStatus)
    return resultado;

  const uint16_t pcInicial = regBank.getPC();
  try {
    uint16_t IR = buscar(regBank, resultado);
    InstrucaoDecodificada instr = decode(IR);
    executar(instr, regBank, ula, resultado);
    resultado.instrucaoExecutada = true;
  } catch (const FaltaPagina &) {
    regBank.setPC(pcInicial);
  }

  return resultado;
}
