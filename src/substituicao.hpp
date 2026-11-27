#ifndef SUBSTITUICAO_HPP
#define SUBSTITUICAO_HPP

#include "config.hpp"
#include <cstddef>
#include <deque>
#include <memory>
#include <vector>

class Substituicao {
public:
    virtual ~Substituicao() = default;

    virtual void registrarCarga(std::size_t quadro) = 0;
    virtual void registrarAcesso(std::size_t quadro) = 0;
    virtual std::size_t selecionarVitima() const = 0;
    virtual void removerQuadro(std::size_t quadro) = 0;
    virtual void limpar() = 0;
};

class SubstituicaoFIFO : public Substituicao {
private:
    std::deque<std::size_t> ordemCarga;

public:
    void registrarCarga(std::size_t quadro) override;
    void registrarAcesso(std::size_t quadro) override;
    std::size_t selecionarVitima() const override;
    void removerQuadro(std::size_t quadro) override;
    void limpar() override;
};

class SubstituicaoLRU : public Substituicao {
private:
    std::vector<std::size_t> ordemUso;

public:
    void registrarCarga(std::size_t quadro) override;
    void registrarAcesso(std::size_t quadro) override;
    std::size_t selecionarVitima() const override;
    void removerQuadro(std::size_t quadro) override;
    void limpar() override;
};

std::unique_ptr<Substituicao> criarSubstituicao(
    PoliticaSubstituicao politica);

#endif // SUBSTITUICAO_HPP
