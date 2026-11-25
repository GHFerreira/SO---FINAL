#ifndef CONTROL_UNIT_HPP
#define CONTROL_UNIT_HPP

#include "../../defines.hpp"
#include "../../mmu.hpp"
#include "../registers/register_bank.hpp"
#include "../ula/ula.hpp"
#include <cstdint>

struct ResultadoPasso {
  bool instrucaoExecutada = false;
  bool faltaPagina = false;
  uint32_t ciclosStall = 0;
  uint32_t acessosRAM = 0;
};

/**
Layout de bits adotado
   [15:12] opcode   | [11:8] regDest | [7:4] regF1 | [3:0] regF2
   Tipo I (LOAD/STORE/BEQ): [7:0] endereco (8 bits)
   JUMP (salto incondicional): [11:0] enderecoAbsoluto (12 bits)
 */
struct InstrucaoDecodificada {
  uint16_t opcode = 0; ///< Bits [15:12] - Código da operação
  uint16_t regDest =
      0; ///< Bits [11:8]  - Registrador de destino (ou Fonte 1, em BEQ)
  uint16_t regF1 = 0; ///< Bits [7:4]   - Primeiro registrador fonte
  uint16_t regF2 = 0; ///< Bits [3:0]   - Segundo registrador fonte
  uint16_t endereco =
      0; ///< Bits [7:0]   - Endereço imediato (Tipo I: LOAD/STORE/BEQ)
  uint16_t enderecoJump =
      0; ///< Bits [11:0] - Endereço absoluto de destino (JUMP)
};

/**
 * ============================================================================
 * @brief Unidade de Controle (UC) da CPU RISC 16-bits
 * ============================================================================
 * O "maestro" da CPU. Não realiza cálculos nem armazena dados por conta
 * própria: a UC apenas orquestra o ciclo de instrução, comandando o
 * RegisterBank, a ULA e a MMU do processo corrente.
 *
 * Responsabilidades (conforme especificação da Prática 0):
 * 1. BUSCA (Fetch)         - Lê a instrução apontada pelo PC na RAM e
 *                            atualiza o Instruction Register (IR) e o PC.
 * 2. DECODIFICAÇÃO (Decode)- Mascara os bits do IR para isolar o opcode
 *                            e os operandos (registradores/endereço).
 * 3. EXECUÇÃO (Execute)    - Aciona a ULA (operações aritméticas/lógicas),
 *                            o RegisterBank (leitura/escrita de operandos)
 *                            ou a Interface de Memória (LOAD/STORE),
 *                            e resolve desvios de fluxo (BEQ/JUMP) e o
 *                            término do job (HALT).
 *
 * Fronteiras respeitadas:
 * - A UC nunca acessa o vetor RAM diretamente; toda transferência passa
 *   pela MMU usando endereços lógicos.
 * - A UC não realiza contas por conta própria; toda aritmética/lógica é
 *   delegada à ULA. A UC apenas decide QUANDO e COM QUAIS operandos a
 *   ULA deve ser acionada, e o que fazer com o resultado.
 */
class ControlUnit {
private:
  MMU &mmu;
  bool haltStatus; ///< Flag de parada: true quando um HALT foi processado

  uint16_t lerMemoria(int pid, uint16_t enderecoLogico,
                      ResultadoPasso &resultado);
  void escreverMemoria(int pid, uint16_t enderecoLogico, uint16_t dado,
                       ResultadoPasso &resultado);
  uint16_t buscar(RegisterBank &regBank, int pid, ResultadoPasso &resultado);
  void executar(const InstrucaoDecodificada &instr, RegisterBank &regBank,
                int pid,
                ULA &ula, ResultadoPasso &resultado);

  /**
   * @brief Despacha uma operação puramente computacional (ADD/SUB/AND/OR)
   * para a ULA, lendo os operandos do RegisterBank e gravando o resultado
   * de volta no registrador de destino.
   */
  void dispararULA(RegisterBank &regBank, ULA &ula, uint16_t opcode,
                   uint16_t regDest, uint16_t regF1, uint16_t regF2);

public:
  explicit ControlUnit(MMU &mmu);

  /** Reinicia a Unidade de Controle (limpa a flag de HALT). */
  void reset();

  /** Retorna true se a UC processou um HALT (job atual finalizado). */
  bool isHalted() const;

  /**
   * @brief Etapa de BUSCA (Fetch).
   * Lê a palavra de 16 bits apontada pelo PC através da Interface de
   * Memória, grava-a no Instruction Register (IR) e incrementa o PC.
   * @return A instrução (IR) recém buscada.
   */
  /**
   * @brief Etapa de DECODIFICAÇÃO (Decode).
   * Isola opcode e operandos da instrução via mascaramento de bits.
   * @param IR Instrução de 16 bits a ser decodificada.
   * @return Estrutura com os campos já separados.
   */
  InstrucaoDecodificada decode(uint16_t IR) const;

  /**
   * @brief Etapa de EXECUÇÃO (Execute).
   * Interpreta o opcode decodificado e comanda ULA / RegisterBank /
   * Interface de Memória para efetivar a ação da instrução.
   * @throws std::runtime_error se o opcode for desconhecido.
   */
  /**
   * @brief Executa um ciclo de instrução completo (Fetch -> Decode -> Execute).
   * Não faz nada se a UC já estiver em estado HALT.
   */
  ResultadoPasso step(int pid, RegisterBank &regBank, ULA &ula);
};

#endif // CONTROL_UNIT_HPP
