#ifndef MMU_HPP
#define MMU_HPP

#include "memoria.hpp"
#include "config.hpp"
#include "substituicao.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

struct ResultadoAcesso {
    uint16_t enderecoFisico = 0;
    bool faltaPagina = false;
    uint32_t ciclosStall = 0;
};

class MMU {
public:
    virtual ~MMU() = default;

    virtual ResultadoAcesso traduzir(int pid, uint16_t enderecoLogico) = 0;
    virtual uint16_t ler(int pid, uint16_t enderecoLogico, ResultadoAcesso* resultado = nullptr) = 0;
    virtual void escrever(int pid, uint16_t enderecoLogico, uint16_t dado, ResultadoAcesso* resultado = nullptr) = 0;
};

struct RegiaoProcesso {
    uint16_t base = 0;
    uint16_t limite = 0;
};

class MMUBaseLimite : public MMU {
private:
    Memoria& memoria;
    std::unordered_map<int, RegiaoProcesso> regioes;

public:
    explicit MMUBaseLimite(Memoria& memoria);

    void registrarProcesso(int pid, uint16_t base, uint16_t limite);
    void removerProcesso(int pid);
    void limparProcessos();
    bool possuiProcesso(int pid) const;

    ResultadoAcesso traduzir(int pid, uint16_t enderecoLogico) override;
    uint16_t ler(int pid, uint16_t enderecoLogico, ResultadoAcesso* resultado = nullptr) override;
    void escrever(int pid, uint16_t enderecoLogico, uint16_t dado, ResultadoAcesso* resultado = nullptr) override;
};

struct EstatisticasMMU {
    uint32_t hits = 0;
    uint32_t faltasPagina = 0;
    uint32_t substituicoesPagina = 0;
};

class MMUPaginada : public MMU {
private:
    struct ProcessoPaginado {
        std::vector<uint16_t> imagem;
        std::vector<int> quadros;
        EstatisticasMMU estatisticas;
    };

    struct Quadro {
        bool ocupado = false;
        int pid = -1;
        uint32_t pagina = 0;
        bool sujo = false;
    };

    Memoria& memoria;
    uint32_t bitsDeslocamento;
    uint32_t tamanhoPagina;
    uint32_t mascaraDeslocamento;
    uint32_t enderecoInicioQuadros;
    std::vector<Quadro> quadros;
    std::unordered_map<int, ProcessoPaginado> processos;
    std::unique_ptr<Substituicao> politica;

    ResultadoAcesso traduzirInterno(int pid, uint16_t enderecoLogico,
                                    bool escrita);
    void carregarPagina(int pid, uint32_t pagina, ProcessoPaginado& processo,
                        EstatisticasMMU& estatisticas);
    void gravarPaginaSuja(const Quadro& quadro);

public:
    MMUPaginada(Memoria& memoria, uint32_t bitsDeslocamento,
                uint32_t quantidadeQuadros,
                PoliticaSubstituicao politicaSubstituicao);

    void registrarProcesso(int pid, const std::vector<uint16_t>& imagem);
    void removerProcesso(int pid);
    void limparProcessos();
    bool possuiProcesso(int pid) const;
    uint32_t getTamanhoPagina() const;
    uint32_t getQuantidadeQuadros() const;
    EstatisticasMMU obterEstatisticas(int pid) const;

    ResultadoAcesso traduzir(int pid, uint16_t enderecoLogico) override;
    uint16_t ler(int pid, uint16_t enderecoLogico,
                 ResultadoAcesso* resultado = nullptr) override;
    void escrever(int pid, uint16_t enderecoLogico, uint16_t dado,
                  ResultadoAcesso* resultado = nullptr) override;
};

#endif // MMU_HPP
