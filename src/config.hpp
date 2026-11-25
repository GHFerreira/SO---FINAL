#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>

enum class PoliticaEscalonamento {
    FCFS,
    SJN,
    ROUND_ROBIN,
    PRIORIDADE
};

enum class ModoExecucao {
    NAO_PREEMPTIVO,
    PREEMPTIVO
};

enum class PoliticaSubstituicao {
    FIFO,
    LRU
};

struct Config {
    uint32_t numeroNucleos = 1;
    PoliticaEscalonamento politica = PoliticaEscalonamento::FCFS;
    ModoExecucao modo = ModoExecucao::NAO_PREEMPTIVO;
    uint32_t quantum = 1;
    uint32_t custoTrocaContexto = 0;
    uint32_t bitsDeslocamento = 4;
    uint32_t quadros = 8;
    PoliticaSubstituicao substituicao = PoliticaSubstituicao::FIFO;
    bool contencaoBarramento = false;
    bool ajudaSolicitada = false;

    static bool analisarArgumentos(int argc, char* argv[], Config& config,
                                   std::vector<std::string>& arquivosJobs,
                                   std::string& erro);
    bool validarExecucaoFase22(std::string& erro) const;
    static std::string textoAjuda();
};

#endif // CONFIG_HPP
