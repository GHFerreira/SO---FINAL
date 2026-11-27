#include "reporter.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>

using namespace std;

namespace {

constexpr int LARGURA_PALAVRA_16_BITS = 4;

void imprimirHex16(ostream& saida, uint16_t valor) {
    saida << "0x"
          << uppercase
          << hex
          << setw(LARGURA_PALAVRA_16_BITS)
          << setfill('0')
          << valor
          << dec
          << nouppercase
          << setfill(' ');
}

void imprimirRelatorio(ostream& saida, const JobMetrics& metricas) {
    saida << "--- ESTADO DA CPU ---\n";

    saida << "PC: ";
    imprimirHex16(saida, metricas.pcFinal);
    
    saida << " IR: ";
    imprimirHex16(saida, metricas.irFinal);
    
    saida << '\n';
    saida << "Registradores Gerais:\n";
    for (int i = 0; i < 8; ++i) {
        saida << 'R' << i << ": ";
        imprimirHex16(saida, metricas.registradores[i]);

        if (i % 4 == 3) {
            saida << '\n';
        } else {
            saida << "   ";
        }
    }

    saida << "--- ESTATISTICAS DO JOB ---\n";
    saida << "Instrucoes Executadas: " << metricas.instrucoesExecutadas << '\n';
    saida << "Acessos a Memoria (RAM): " << metricas.acessosRAM << '\n';
    saida << "Acessos ao Disco (I/O): " << metricas.acessosDisco << '\n';
    saida << "Acertos de memoria: " << metricas.hitsMemoria << '\n';
    saida << "Faltas de pagina: " << metricas.faltasPagina << '\n';
    saida << "Substituicoes de pagina: "
          << metricas.substituicoesPagina << '\n';
    saida << "CLOCK TOTAL DESTE JOB: " << metricas.clockTotal << " ciclos\n";
}

} // namespace

void Reporter::exibirTerminal(const JobMetrics& metricas) {
    imprimirRelatorio(cout, metricas);
}

void Reporter::salvarOutputDat(const string& caminhoArquivo, const JobMetrics& metricas) {
    ofstream arquivo(caminhoArquivo, ios::app);

    if (!arquivo.is_open()) {
        cerr << "[REPORTER] Falha ao abrir arquivo de saida: "
                  << caminhoArquivo << '\n';
        return;
    }

    imprimirRelatorio(arquivo, metricas);
    arquivo << "===================================================\n";
}
