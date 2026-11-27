#include "config.hpp"
#include "defines.hpp"
#include <algorithm>
#include <cctype>
#include <limits>

using namespace std;

namespace {
string minusculas(string texto) {
    transform(texto.begin(), texto.end(), texto.begin(),
                   [](unsigned char caractere) {
                       return static_cast<char>(tolower(caractere));
                   });
    return texto;
}

bool converterNumero(const string& texto, uint32_t& valor) {
    if (texto.empty() || texto[0] == '-') {
        return false;
    }

    try {
        size_t caracteresLidos = 0;
        unsigned long long numero = stoull(texto, &caracteresLidos);
        if (caracteresLidos != texto.size() ||
            numero > numeric_limits<uint32_t>::max()) {
            return false;
        }
        valor = static_cast<uint32_t>(numero);
        return true;
    } catch (...) {
        return false;
    }
}

bool obterValor(int& indice, int argc, char* argv[],
                const string& valorInline, string& valor,
                string& erro, const string& opcao) {
    if (!valorInline.empty()) {
        valor = valorInline;
        return true;
    }
    if (indice + 1 >= argc || string(argv[indice + 1]).rfind("--", 0) == 0) {
        erro = "Falta valor para " + opcao + ".";
        return false;
    }
    valor = argv[++indice];
    return true;
}

bool lerNumero(int& indice, int argc, char* argv[],
               const string& valorInline, uint32_t& destino,
               string& erro, const string& opcao) {
    string valor;
    if (!obterValor(indice, argc, argv, valorInline, valor, erro, opcao)) {
        return false;
    }
    if (!converterNumero(valor, destino)) {
        erro = "Valor numerico invalido para " + opcao + ": " + valor + ".";
        return false;
    }
    return true;
}

bool valorPositivo(uint32_t valor, string& erro,
                   const string& opcao) {
    if (valor == 0) {
        erro = opcao + " deve ser maior que zero.";
        return false;
    }
    return true;
}
}

bool Config::analisarArgumentos(int argc, char* argv[], Config& config,
                                vector<string>& arquivosJobs,
                                string& erro) {
    arquivosJobs.clear();
    erro.clear();

    for (int indice = 1; indice < argc; ++indice) {
        string argumento = argv[indice];
        if (argumento == "--help" || argumento == "-h") {
            config.ajudaSolicitada = true;
            continue;
        }
        if (argumento.rfind("--", 0) != 0) {
            arquivosJobs.push_back(argumento);
            continue;
        }

        size_t separador = argumento.find('=');
        string opcao = argumento.substr(0, separador);
        string valorInline;
        if (separador != string::npos) {
            valorInline = argumento.substr(separador + 1);
            if (valorInline.empty()) {
                erro = "Falta valor para " + opcao + ".";
                return false;
            }
        }

        if (opcao == "--bus-contention") {
            if (valorInline.empty()) {
                config.contencaoBarramento = true;
            } else {
                string valor = minusculas(valorInline);
                if (valor != "true" && valor != "false") {
                    erro = "Use true ou false para --bus-contention.";
                    return false;
                }
                config.contencaoBarramento = valor == "true";
            }
            continue;
        }

        if (opcao == "--cores") {
            if (!lerNumero(indice, argc, argv, valorInline, config.numeroNucleos,
                           erro, opcao) ||
                !valorPositivo(config.numeroNucleos, erro, opcao)) return false;
        } else if (opcao == "--quantum") {
            if (!lerNumero(indice, argc, argv, valorInline, config.quantum,
                           erro, opcao) ||
                !valorPositivo(config.quantum, erro, opcao)) return false;
        } else if (opcao == "--ctx-cost") {
            if (!lerNumero(indice, argc, argv, valorInline,
                           config.custoTrocaContexto, erro, opcao)) return false;
        } else if (opcao == "--bits-offset") {
            if (!lerNumero(indice, argc, argv, valorInline,
                           config.bitsDeslocamento, erro, opcao)) return false;
            if (config.bitsDeslocamento < 1 || config.bitsDeslocamento > 8) {
                erro = "--bits-offset deve estar entre 1 e 8.";
                return false;
            }
        } else if (opcao == "--frames") {
            if (!lerNumero(indice, argc, argv, valorInline, config.quadros,
                           erro, opcao) ||
                !valorPositivo(config.quadros, erro, opcao)) return false;
        } else if (opcao == "--policy") {
            string valor;
            if (!obterValor(indice, argc, argv, valorInline, valor, erro, opcao)) {
                return false;
            }
            valor = minusculas(valor);
            if (valor == "fcfs") {
                config.politica = PoliticaEscalonamento::FCFS;
            } else if (valor == "sjn") {
                config.politica = PoliticaEscalonamento::SJN;
            } else if (valor == "rr" || valor == "round-robin") {
                config.politica = PoliticaEscalonamento::ROUND_ROBIN;
            } else if (valor == "priority" || valor == "prioridade") {
                config.politica = PoliticaEscalonamento::PRIORIDADE;
            } else {
                erro = "Politica invalida: " + valor + ". Use fcfs, sjn, rr ou priority.";
                return false;
            }
        } else if (opcao == "--mode") {
            string valor;
            if (!obterValor(indice, argc, argv, valorInline, valor, erro, opcao)) {
                return false;
            }
            valor = minusculas(valor);
            if (valor == "non-preemptive" || valor == "nao-preemptivo") {
                config.modo = ModoExecucao::NAO_PREEMPTIVO;
            } else if (valor == "preemptive" || valor == "preemptivo") {
                config.modo = ModoExecucao::PREEMPTIVO;
            } else {
                erro = "Modo invalido: " + valor + ". Use non-preemptive ou preemptive.";
                return false;
            }
        } else if (opcao == "--replacement") {
            string valor;
            if (!obterValor(indice, argc, argv, valorInline, valor, erro, opcao)) {
                return false;
            }
            valor = minusculas(valor);
            if (valor == "fifo") {
                config.substituicao = PoliticaSubstituicao::FIFO;
            } else if (valor == "lru") {
                config.substituicao = PoliticaSubstituicao::LRU;
            } else {
                erro = "Substituicao invalida: " + valor + ". Use fifo ou lru.";
                return false;
            }
        } else {
            erro = "Opcao desconhecida: " + opcao + ".";
            return false;
        }
    }

    return true;
}

bool Config::validarExecucaoFase42(string& erro) const {
    erro.clear();
    if (numeroNucleos == 0) {
        erro = "--cores deve ser maior que zero.";
    } else if (bitsDeslocamento < 1 || bitsDeslocamento > 8) {
        erro = "--bits-offset deve estar entre 1 e 8.";
    } else if (quadros == 0) {
        erro = "--frames deve ser maior que zero.";
    } else if (quadros > (TAM_RAM - OS_RESERVED_MEM) /
                            (1u << bitsDeslocamento)) {
        erro = "--frames excede os quadros disponiveis na RAM.";
    }

    return erro.empty();
}

string Config::textoAjuda() {
    return "Uso: simulador [opcoes] [arquivos de jobs...]\n"
           "  --cores N             numero de nucleos (padrao: 1)\n"
           "  --policy P            fcfs, sjn, rr ou priority (padrao: fcfs)\n"
           "  --mode M              non-preemptive ou preemptive (padrao: non-preemptive)\n"
           "  --quantum N           quantum em ciclos (padrao: 1)\n"
           "  --ctx-cost N          custo da troca de contexto (padrao: 0)\n"
           "  --bits-offset N       bits de deslocamento, de 1 a 8 (padrao: 4)\n"
           "  --frames N            quantidade de quadros (padrao: 8)\n"
           "  --replacement P       fifo ou lru (padrao: fifo)\n"
           "  --bus-contention      ativa disputa pelo barramento (padrao: desativada)\n"
           "  --help                mostra esta ajuda\n";
}
