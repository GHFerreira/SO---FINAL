#include "../control/control_unit.hpp"
#include "../registers/register_bank.hpp"
#include "../ula/ula.hpp"
#include "../../defines.hpp"
#include "../../memoria.hpp"
#include "../../mmu.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#define TEST_ASSERT(condition)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << "[FALHA] " << __FILE__ << ":" << __LINE__                   \
                << " Assercao falhou: " #condition << std::endl;               \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

struct AmbienteTeste {
  Memoria memoria;
  MMUBaseLimite mmu;
  ControlUnit uc;
  RegisterBank registradores;
  ULA ula;

  AmbienteTeste() : memoria(TAM_RAM), mmu(memoria), uc(mmu, 1) {
    mmu.registrarProcesso(1, OS_RESERVED_MEM,
                          static_cast<uint16_t>(TAM_RAM - OS_RESERVED_MEM));
  }

  void gravar(uint16_t enderecoLogico, uint16_t dado) {
    memoria.escreverKernel(OS_RESERVED_MEM + enderecoLogico, dado);
  }
};

void testDecode() {
  AmbienteTeste ambiente;
  InstrucaoDecodificada instr = ambiente.uc.decode(0x124F);
  TEST_ASSERT(instr.opcode == OP_LOAD);
  TEST_ASSERT(instr.regDest == 2);
  TEST_ASSERT(instr.endereco == 0x4F);

  InstrucaoDecodificada jmp = ambiente.uc.decode(0x80A0);
  TEST_ASSERT(jmp.opcode == OP_JUMP);
  TEST_ASSERT(jmp.enderecoJump == 0x0A0);
}

void testFetchLogico() {
  AmbienteTeste ambiente;
  ambiente.gravar(0, 0xF000);

  ResultadoPasso resultado = ambiente.uc.step(ambiente.registradores, ambiente.ula);
  TEST_ASSERT(resultado.instrucaoExecutada);
  TEST_ASSERT(ambiente.registradores.getIR() == 0xF000);
  TEST_ASSERT(ambiente.registradores.getPC() == 1);
  TEST_ASSERT(ambiente.uc.isHalted());
}

void testCicloCompletoSoma() {
  AmbienteTeste ambiente;
  ambiente.gravar(0, 0x100A);
  ambiente.gravar(1, 0x110B);
  ambiente.gravar(2, 0x2201);
  ambiente.gravar(3, 0x320C);
  ambiente.gravar(4, 0xF000);
  ambiente.gravar(10, 5);
  ambiente.gravar(11, 7);

  ambiente.uc.step(ambiente.registradores, ambiente.ula);
  ambiente.uc.step(ambiente.registradores, ambiente.ula);
  ambiente.uc.step(ambiente.registradores, ambiente.ula);
  ambiente.uc.step(ambiente.registradores, ambiente.ula);
  TEST_ASSERT(ambiente.memoria.ler(OS_RESERVED_MEM + 12) == 12);
  ambiente.uc.step(ambiente.registradores, ambiente.ula);
  TEST_ASSERT(ambiente.uc.isHalted());
}

void testDesviosLogicos() {
  AmbienteTeste ambiente;
  ambiente.gravar(0, 0x7014);
  ambiente.gravar(20, 0x81FF);

  ambiente.uc.step(ambiente.registradores, ambiente.ula);
  TEST_ASSERT(ambiente.registradores.getPC() == 20);
  ambiente.uc.step(ambiente.registradores, ambiente.ula);
  TEST_ASSERT(ambiente.registradores.getPC() == 0x1FF);
}

void testOpcodeInvalido() {
  AmbienteTeste ambiente;
  ambiente.gravar(0, 0x9000);
  bool lancou = false;
  try {
    ambiente.uc.step(ambiente.registradores, ambiente.ula);
  } catch (const std::runtime_error &) {
    lancou = true;
  }
  TEST_ASSERT(lancou);
}

int main() {
  testDecode();
  testFetchLogico();
  testCicloCompletoSoma();
  testDesviosLogicos();
  testOpcodeInvalido();
  std::cout << "Todos os testes da unidade de controle passaram!\n";
  return 0;
}
