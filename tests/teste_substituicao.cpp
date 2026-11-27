#include "../src/substituicao.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

struct ResultadoReferencias {
    uint32_t faltas = 0;
    vector<int> paginasEmQuadros;
};

ResultadoReferencias simularReferencias(
    const vector<int>& referencias, size_t quantidadeQuadros,
    Substituicao& politica) {
    if (quantidadeQuadros == 0) {
        throw invalid_argument("A simulacao precisa de quadros.");
    }

    ResultadoReferencias resultado;
    resultado.paginasEmQuadros.assign(quantidadeQuadros, -1);

    for (int pagina : referencias) {
        size_t quadro = quantidadeQuadros;
        for (size_t indice = 0; indice < quantidadeQuadros; ++indice) {
            if (resultado.paginasEmQuadros[indice] == pagina) {
                quadro = indice;
                break;
            }
        }

        if (quadro != quantidadeQuadros) {
            politica.registrarAcesso(quadro);
            continue;
        }

        ++resultado.faltas;
        for (size_t indice = 0; indice < quantidadeQuadros; ++indice) {
            if (resultado.paginasEmQuadros[indice] == -1) {
                quadro = indice;
                break;
            }
        }

        if (quadro == quantidadeQuadros) {
            quadro = politica.selecionarVitima();
            resultado.paginasEmQuadros[quadro] = -1;
            politica.removerQuadro(quadro);
        }

        resultado.paginasEmQuadros[quadro] = pagina;
        politica.registrarCarga(quadro);
    }

    return resultado;
}

void testarOraculosClassicos() {
    const vector<int> referencias = {
        7, 0, 1, 2, 0, 3, 0, 4, 2, 3,
        0, 3, 2, 1, 2, 0, 1, 7, 0, 1
    };

    SubstituicaoFIFO fifo;
    ResultadoReferencias resultadoFIFO =
        simularReferencias(referencias, 3, fifo);
    assert(resultadoFIFO.faltas == 15);

    SubstituicaoLRU lru;
    ResultadoReferencias resultadoLRU =
        simularReferencias(referencias, 3, lru);
    assert(resultadoLRU.faltas == 12);
}

void testarAnomaliaDeBelady() {
    const vector<int> referencias = {
        1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5
    };

    SubstituicaoFIFO fifoTresQuadros;
    ResultadoReferencias resultadoTres =
        simularReferencias(referencias, 3, fifoTresQuadros);
    assert(resultadoTres.faltas == 9);

    SubstituicaoFIFO fifoQuatroQuadros;
    ResultadoReferencias resultadoQuatro =
        simularReferencias(referencias, 4, fifoQuatroQuadros);
    assert(resultadoQuatro.faltas == 10);
}

void testarAcessoRecenteAlteraVitimaDoLRU() {
    const vector<int> referencias = {1, 2, 1, 3};

    SubstituicaoFIFO fifo;
    ResultadoReferencias resultadoFIFO =
        simularReferencias(referencias, 2, fifo);
    assert(resultadoFIFO.paginasEmQuadros[0] == 3);
    assert(resultadoFIFO.paginasEmQuadros[1] == 2);

    SubstituicaoLRU lru;
    ResultadoReferencias resultadoLRU =
        simularReferencias(referencias, 2, lru);
    assert(resultadoLRU.paginasEmQuadros[0] == 1);
    assert(resultadoLRU.paginasEmQuadros[1] == 3);
}

void testarEstadosInvalidosEFactory() {
    SubstituicaoFIFO fifo;
    bool falhouVitimaVazia = false;
    try {
        fifo.selecionarVitima();
    } catch (const logic_error&) {
        falhouVitimaVazia = true;
    }
    assert(falhouVitimaVazia);

    bool falhouAcessoAusente = false;
    try {
        fifo.registrarAcesso(0);
    } catch (const invalid_argument&) {
        falhouAcessoAusente = true;
    }
    assert(falhouAcessoAusente);

    fifo.registrarCarga(0);
    assert(fifo.selecionarVitima() == 0);
    fifo.limpar();
    assert(criarSubstituicao(PoliticaSubstituicao::FIFO) != nullptr);
    assert(criarSubstituicao(PoliticaSubstituicao::LRU) != nullptr);
}

int main() {
    testarOraculosClassicos();
    testarAnomaliaDeBelady();
    testarAcessoRecenteAlteraVitimaDoLRU();
    testarEstadosInvalidosEFactory();
    cout << "Todos os testes de substituicao passaram!\n";
    return 0;
}
