#include "mmu.hpp"
#include "defines.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace std;

MMUBaseLimite::MMUBaseLimite(Memoria& memoria) : memoria(memoria) {}

void MMUBaseLimite::registrarProcesso(int pid, uint16_t base, uint16_t limite) {
    if (base < OS_RESERVED_MEM) {
        throw invalid_argument("Base do processo invade area reservada do SO.");
    }
    if (limite == 0) {
        throw invalid_argument("Limite do processo deve ser maior que zero.");
    }
    uint32_t fimRegiao = static_cast<uint32_t>(base) + static_cast<uint32_t>(limite);
    if (fimRegiao > memoria.getTamanho()) {
        throw invalid_argument("Regiao do processo excede o tamanho da RAM.");
    }

    RegiaoProcesso regiao;
    regiao.base = base;
    regiao.limite = limite;
    regioes[pid] = regiao;
}

void MMUBaseLimite::removerProcesso(int pid) {
    regioes.erase(pid);
}

void MMUBaseLimite::limparProcessos() {
    regioes.clear();
}

bool MMUBaseLimite::possuiProcesso(int pid) const {
    return regioes.find(pid) != regioes.end();
}

ResultadoAcesso MMUBaseLimite::traduzir(int pid, uint16_t enderecoLogico) {
    auto it = regioes.find(pid);
    if (it == regioes.end()) {
        throw runtime_error("SEGFAULT: Processo nao registrado na MMU!");
    }

    const RegiaoProcesso& regiao = it->second;
    if (enderecoLogico >= regiao.limite) {
        throw runtime_error("SEGFAULT: Endereco logico fora do limite do processo!");
    }

    uint32_t enderecoFisico = static_cast<uint32_t>(regiao.base) + static_cast<uint32_t>(enderecoLogico);
    if (enderecoFisico < OS_RESERVED_MEM || enderecoFisico >= memoria.getTamanho()) {
        throw runtime_error("SEGFAULT: Endereco fisico invalido!");
    }

    ResultadoAcesso resultado;
    resultado.enderecoFisico = static_cast<uint16_t>(enderecoFisico);
    resultado.faltaPagina = false;
    resultado.ciclosStall = 0;
    return resultado;
}

uint16_t MMUBaseLimite::ler(int pid, uint16_t enderecoLogico, ResultadoAcesso* resultado) {
    ResultadoAcesso acesso = traduzir(pid, enderecoLogico);
    if (resultado != nullptr) {
        *resultado = acesso;
    }
    return memoria.ler(acesso.enderecoFisico);
}

void MMUBaseLimite::escrever(int pid, uint16_t enderecoLogico, uint16_t dado, ResultadoAcesso* resultado) {
    ResultadoAcesso acesso = traduzir(pid, enderecoLogico);
    if (resultado != nullptr) {
        *resultado = acesso;
    }
    memoria.escrever(acesso.enderecoFisico, dado);
}

MMUPaginada::MMUPaginada(Memoria& memoria, uint32_t bitsDeslocamento,
                         uint32_t quantidadeQuadros,
                         PoliticaSubstituicao politicaSubstituicao)
    : memoria(memoria), bitsDeslocamento(bitsDeslocamento),
      tamanhoPagina(0), mascaraDeslocamento(0),
      enderecoInicioQuadros(OS_RESERVED_MEM), quadros(quantidadeQuadros),
      politica(criarSubstituicao(politicaSubstituicao)) {
    if (bitsDeslocamento < 1 || bitsDeslocamento > 8) {
        throw invalid_argument("Bits de deslocamento devem estar entre 1 e 8.");
    }
    if (quantidadeQuadros == 0) {
        throw invalid_argument("A MMU paginada precisa de ao menos um quadro.");
    }
    if (memoria.getTamanho() <= enderecoInicioQuadros) {
        throw invalid_argument("RAM insuficiente para a area reservada do SO.");
    }

    tamanhoPagina = 1u << bitsDeslocamento;
    mascaraDeslocamento = tamanhoPagina - 1;
    const uint32_t quadrosDisponiveis =
        (memoria.getTamanho() - enderecoInicioQuadros) / tamanhoPagina;
    if (quantidadeQuadros > quadrosDisponiveis) {
        throw invalid_argument("Quantidade de quadros excede a RAM disponivel.");
    }
}

void MMUPaginada::registrarProcesso(
    int pid, const vector<uint16_t>& imagem) {
    if (imagem.empty() || imagem.size() >
                              static_cast<size_t>(numeric_limits<uint16_t>::max()) + 1) {
        throw invalid_argument("Tamanho invalido para imagem do processo.");
    }

    ProcessoPaginado processo;
    processo.imagem = imagem;
    const size_t quantidadePaginas =
        (imagem.size() + tamanhoPagina - 1) / tamanhoPagina;
    processo.quadros.assign(quantidadePaginas, -1);
    processos[pid] = move(processo);
}

void MMUPaginada::removerProcesso(int pid) {
    auto encontrado = processos.find(pid);
    if (encontrado == processos.end()) {
        return;
    }

    for (size_t pagina = 0; pagina < encontrado->second.quadros.size(); ++pagina) {
        const int indiceQuadro = encontrado->second.quadros[pagina];
        if (indiceQuadro < 0) {
            continue;
        }
        Quadro& quadro = quadros[static_cast<size_t>(indiceQuadro)];
        gravarPaginaSuja(quadro);
        politica->removerQuadro(static_cast<size_t>(indiceQuadro));
        quadro = Quadro{};
    }
    processos.erase(encontrado);
}

void MMUPaginada::limparProcessos() {
    processos.clear();
    for (Quadro& quadro : quadros) {
        quadro = Quadro{};
    }
    politica->limpar();
}

bool MMUPaginada::possuiProcesso(int pid) const {
    return processos.find(pid) != processos.end();
}

uint32_t MMUPaginada::getTamanhoPagina() const {
    return tamanhoPagina;
}

uint32_t MMUPaginada::getQuantidadeQuadros() const {
    return static_cast<uint32_t>(quadros.size());
}

EstatisticasMMU MMUPaginada::obterEstatisticas(int pid) const {
    auto encontrado = processos.find(pid);
    if (encontrado == processos.end()) {
        throw runtime_error("Processo nao registrado na MMU paginada.");
    }
    return encontrado->second.estatisticas;
}

void MMUPaginada::gravarPaginaSuja(const Quadro& quadro) {
    if (!quadro.ocupado || !quadro.sujo) {
        return;
    }
    auto encontrado = processos.find(quadro.pid);
    if (encontrado == processos.end()) {
        throw logic_error("Quadro sujo pertence a processo inexistente.");
    }

    ProcessoPaginado& processo = encontrado->second;
    const uint32_t baseLogica = quadro.pagina * tamanhoPagina;
    const uint32_t baseFisica = enderecoInicioQuadros +
                                static_cast<uint32_t>(
                                    processo.quadros[quadro.pagina]) * tamanhoPagina;
    for (uint32_t deslocamento = 0; deslocamento < tamanhoPagina; ++deslocamento) {
        const uint32_t enderecoLogico = baseLogica + deslocamento;
        if (enderecoLogico >= processo.imagem.size()) {
            break;
        }
        processo.imagem[enderecoLogico] = memoria.lerKernel(
            static_cast<uint16_t>(baseFisica + deslocamento));
    }
}

void MMUPaginada::carregarPagina(int pid, uint32_t pagina,
                                 ProcessoPaginado& processo,
                                 EstatisticasMMU& estatisticas) {
    size_t indiceQuadro = quadros.size();
    for (size_t indice = 0; indice < quadros.size(); ++indice) {
        if (!quadros[indice].ocupado) {
            indiceQuadro = indice;
            break;
        }
    }

    if (indiceQuadro == quadros.size()) {
        indiceQuadro = politica->selecionarVitima();
        Quadro& vitima = quadros[indiceQuadro];
        gravarPaginaSuja(vitima);
        auto processoVitima = processos.find(vitima.pid);
        if (processoVitima == processos.end()) {
            throw logic_error("Vitima pertence a processo inexistente.");
        }
        processoVitima->second.quadros[vitima.pagina] = -1;
        politica->removerQuadro(indiceQuadro);
        vitima = Quadro{};
        ++estatisticas.substituicoesPagina;
    }

    const uint32_t baseFisica = enderecoInicioQuadros +
                                static_cast<uint32_t>(indiceQuadro) * tamanhoPagina;
    const uint32_t baseLogica = pagina * tamanhoPagina;
    for (uint32_t deslocamento = 0; deslocamento < tamanhoPagina; ++deslocamento) {
        const uint32_t enderecoLogico = baseLogica + deslocamento;
        const uint16_t dado = enderecoLogico < processo.imagem.size()
                                  ? processo.imagem[enderecoLogico]
                                  : 0;
        memoria.escreverKernel(static_cast<uint16_t>(baseFisica + deslocamento),
                               dado);
    }

    Quadro& quadro = quadros[indiceQuadro];
    quadro.ocupado = true;
    quadro.pid = pid;
    quadro.pagina = pagina;
    quadro.sujo = false;
    processo.quadros[pagina] = static_cast<int>(indiceQuadro);
    politica->registrarCarga(indiceQuadro);
}

ResultadoAcesso MMUPaginada::traduzirInterno(int pid,
                                              uint16_t enderecoLogico,
                                              bool escrita) {
    auto encontrado = processos.find(pid);
    if (encontrado == processos.end()) {
        throw runtime_error("SEGFAULT: Processo nao registrado na MMU paginada!");
    }
    ProcessoPaginado& processo = encontrado->second;
    if (enderecoLogico >= processo.imagem.size()) {
        throw runtime_error("SEGFAULT: Endereco logico fora do limite do processo!");
    }

    const uint32_t pagina = enderecoLogico >> bitsDeslocamento;
    const uint32_t deslocamento = enderecoLogico & mascaraDeslocamento;
    int indiceQuadro = processo.quadros[pagina];
    if (indiceQuadro < 0) {
        ++processo.estatisticas.faltasPagina;
        carregarPagina(pid, pagina, processo, processo.estatisticas);
        ResultadoAcesso resultado;
        resultado.faltaPagina = true;
        resultado.ciclosStall = 50;
        return resultado;
    }

    ++processo.estatisticas.hits;
    politica->registrarAcesso(static_cast<size_t>(indiceQuadro));
    Quadro& quadro = quadros[static_cast<size_t>(indiceQuadro)];
    if (escrita) {
        quadro.sujo = true;
    }
    ResultadoAcesso resultado;
    resultado.enderecoFisico = static_cast<uint16_t>(
        enderecoInicioQuadros + static_cast<uint32_t>(indiceQuadro) * tamanhoPagina +
        deslocamento);
    return resultado;
}

ResultadoAcesso MMUPaginada::traduzir(int pid, uint16_t enderecoLogico) {
    return traduzirInterno(pid, enderecoLogico, false);
}

uint16_t MMUPaginada::ler(int pid, uint16_t enderecoLogico,
                          ResultadoAcesso* resultado) {
    ResultadoAcesso acesso = traduzirInterno(pid, enderecoLogico, false);
    if (resultado != nullptr) {
        *resultado = acesso;
    }
    if (acesso.faltaPagina) {
        return 0;
    }
    return memoria.ler(acesso.enderecoFisico);
}

void MMUPaginada::escrever(int pid, uint16_t enderecoLogico, uint16_t dado,
                           ResultadoAcesso* resultado) {
    ResultadoAcesso acesso = traduzirInterno(pid, enderecoLogico, true);
    if (resultado != nullptr) {
        *resultado = acesso;
    }
    if (acesso.faltaPagina) {
        return;
    }
    memoria.escrever(acesso.enderecoFisico, dado);
}
