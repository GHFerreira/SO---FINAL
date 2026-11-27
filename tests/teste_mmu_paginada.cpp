#include "../src/mmu.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

void testarFatiamentoNosLimitesDeBits() {
    for (uint32_t bits = 1; bits <= 8; ++bits) {
        Memoria memoria;
        MMUPaginada mmu(memoria, bits, 2, PoliticaSubstituicao::FIFO);
        vector<uint16_t> imagem(1024, 0);
        imagem[0] = 0x1234;
        imagem[255] = 0xABCD;
        mmu.registrarProcesso(1, imagem);
        assert(mmu.getTamanhoPagina() == (1u << bits));

        const uint16_t endereco = static_cast<uint16_t>(1u << bits);
        ResultadoAcesso falta;
        assert(mmu.ler(1, endereco, &falta) == 0);
        assert(falta.faltaPagina);
        assert(falta.ciclosStall == 50);

        ResultadoAcesso hit;
        mmu.ler(1, endereco, &hit);
        assert(!hit.faltaPagina);
        assert(hit.enderecoFisico >= OS_RESERVED_MEM);

        ResultadoAcesso offset = mmu.traduzir(
            1, static_cast<uint16_t>(endereco + mmu.getTamanhoPagina() - 1));
        assert(offset.enderecoFisico == hit.enderecoFisico +
                                           mmu.getTamanhoPagina() - 1);

        ResultadoAcesso outraPagina;
        mmu.ler(1, 0, &outraPagina);
        assert(outraPagina.faltaPagina);
        mmu.ler(1, 0, &outraPagina);
        assert(!outraPagina.faltaPagina);
        assert(outraPagina.enderecoFisico != hit.enderecoFisico);
    }
}

void testarFaltaHitSubstituicaoEWriteBack() {
    Memoria memoria;
    MMUPaginada mmu(memoria, 2, 2, PoliticaSubstituicao::FIFO);
    vector<uint16_t> imagem(12, 0);
    imagem[5] = 0x1234;
    imagem[9] = 0x5678;
    mmu.registrarProcesso(7, imagem);

    ResultadoAcesso acesso;
    assert(mmu.ler(7, 5, &acesso) == 0);
    assert(acesso.faltaPagina && acesso.ciclosStall == 50);
    assert(memoria.getTotalAcessos() == 0);
    assert(mmu.ler(7, 5, &acesso) == 0x1234);
    assert(!acesso.faltaPagina);
    mmu.escrever(7, 5, 0xABCD, &acesso);
    assert(!acesso.faltaPagina);

    mmu.ler(7, 9, &acesso);
    assert(acesso.faltaPagina);
    mmu.ler(7, 9, &acesso);
    mmu.ler(7, 1, &acesso);
    assert(acesso.faltaPagina);
    mmu.ler(7, 1, &acesso);
    mmu.ler(7, 5, &acesso);
    assert(acesso.faltaPagina);
    assert(mmu.ler(7, 5, &acesso) == 0xABCD);

    EstatisticasMMU estatisticas = mmu.obterEstatisticas(7);
    assert(estatisticas.faltasPagina == 4);
    assert(estatisticas.substituicoesPagina == 2);
    assert(estatisticas.hits == 5);
}

void testarSubstituicaoFIFOELRU() {
    for (PoliticaSubstituicao politica :
         {PoliticaSubstituicao::FIFO, PoliticaSubstituicao::LRU}) {
        Memoria memoria;
        MMUPaginada mmu(memoria, 1, 2, politica);
        vector<uint16_t> imagem(8, 0);
        mmu.registrarProcesso(1, imagem);

        ResultadoAcesso acesso;
        mmu.ler(1, 0, &acesso);
        mmu.ler(1, 0, &acesso);
        mmu.ler(1, 2, &acesso);
        mmu.ler(1, 2, &acesso);
        mmu.ler(1, 0, &acesso);
        assert(!acesso.faltaPagina);
        mmu.ler(1, 4, &acesso);
        assert(acesso.faltaPagina);

        const uint16_t enderecoVitima =
            politica == PoliticaSubstituicao::FIFO ? 0 : 2;
        mmu.ler(1, enderecoVitima, &acesso);
        assert(acesso.faltaPagina);
    }
}

void testarLimitesERAM() {
    Memoria memoria;
    MMUPaginada mmu(memoria, 4, 8, PoliticaSubstituicao::FIFO);
    mmu.registrarProcesso(1, vector<uint16_t>(16, 0));

    bool falhouLimite = false;
    try {
        mmu.ler(1, 16);
    } catch (const runtime_error&) {
        falhouLimite = true;
    }
    assert(falhouLimite);

    bool falhouQuadros = false;
    try {
        MMUPaginada mmuInvalida(memoria, 8, 256, PoliticaSubstituicao::FIFO);
    } catch (const invalid_argument&) {
        falhouQuadros = true;
    }
    assert(falhouQuadros);
}

int main() {
    testarFatiamentoNosLimitesDeBits();
    testarFaltaHitSubstituicaoEWriteBack();
    testarSubstituicaoFIFOELRU();
    testarLimitesERAM();
    cout << "Todos os testes da MMU paginada passaram!\n";
    return 0;
}
