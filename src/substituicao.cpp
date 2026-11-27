#include "substituicao.hpp"
#include <algorithm>
#include <stdexcept>

using namespace std;

void SubstituicaoFIFO::registrarCarga(size_t quadro) {
    if (find(ordemCarga.begin(), ordemCarga.end(), quadro) !=
        ordemCarga.end()) {
        throw invalid_argument("Quadro ja registrado no FIFO.");
    }
    ordemCarga.push_back(quadro);
}

void SubstituicaoFIFO::registrarAcesso(size_t quadro) {
    if (find(ordemCarga.begin(), ordemCarga.end(), quadro) ==
        ordemCarga.end()) {
        throw invalid_argument("Acesso a quadro ausente no FIFO.");
    }
}

size_t SubstituicaoFIFO::selecionarVitima() const {
    if (ordemCarga.empty()) {
        throw logic_error("Nao ha quadros residentes no FIFO.");
    }
    return ordemCarga.front();
}

void SubstituicaoFIFO::removerQuadro(size_t quadro) {
    auto encontrado = find(ordemCarga.begin(), ordemCarga.end(), quadro);
    if (encontrado == ordemCarga.end()) {
        throw invalid_argument("Remocao de quadro ausente no FIFO.");
    }
    ordemCarga.erase(encontrado);
}

void SubstituicaoFIFO::limpar() {
    ordemCarga.clear();
}

void SubstituicaoLRU::registrarCarga(size_t quadro) {
    if (find(ordemUso.begin(), ordemUso.end(), quadro) != ordemUso.end()) {
        throw invalid_argument("Quadro ja registrado no LRU.");
    }
    ordemUso.push_back(quadro);
}

void SubstituicaoLRU::registrarAcesso(size_t quadro) {
    auto encontrado = find(ordemUso.begin(), ordemUso.end(), quadro);
    if (encontrado == ordemUso.end()) {
        throw invalid_argument("Acesso a quadro ausente no LRU.");
    }
    ordemUso.erase(encontrado);
    ordemUso.push_back(quadro);
}

size_t SubstituicaoLRU::selecionarVitima() const {
    if (ordemUso.empty()) {
        throw logic_error("Nao ha quadros residentes no LRU.");
    }
    return ordemUso.front();
}

void SubstituicaoLRU::removerQuadro(size_t quadro) {
    auto encontrado = find(ordemUso.begin(), ordemUso.end(), quadro);
    if (encontrado == ordemUso.end()) {
        throw invalid_argument("Remocao de quadro ausente no LRU.");
    }
    ordemUso.erase(encontrado);
}

void SubstituicaoLRU::limpar() {
    ordemUso.clear();
}

unique_ptr<Substituicao> criarSubstituicao(
    PoliticaSubstituicao politica) {
    switch (politica) {
        case PoliticaSubstituicao::FIFO:
            return make_unique<SubstituicaoFIFO>();
        case PoliticaSubstituicao::LRU:
            return make_unique<SubstituicaoLRU>();
    }
    throw invalid_argument("Politica de substituicao desconhecida.");
}
