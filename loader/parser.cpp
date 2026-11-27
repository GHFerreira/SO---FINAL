#include "parser.hpp"
#include "clock_core.hpp"
#include "metrics.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

using namespace std;

bool JobParser::carregarArquivo(const string& caminho, JobData& outJob) {
    ifstream arquivo(caminho);
    if (!arquivo.is_open()) {
        cerr << "Erro: Nao foi possivel abrir o arquivo " << caminho << endl;
        return false;
    }

    outJob.nomeArquivo = caminho;
    outJob.prioridade = 0;
    outJob.burst = 0;
    outJob.instrucoes.clear();
    outJob.variaveis.clear();

    string linha;
    bool inDataSection = false;
    bool burstDefinido = false;

    while (getline(arquivo, linha)) {
        // Remove comentarios (tudo a partir de //)
        size_t posComentario = linha.find("//");
        if (posComentario != string::npos) {
            linha = linha.substr(0, posComentario);
        }

        // Limpa espacos em branco nas pontas
        linha.erase(linha.begin(), find_if(linha.begin(), linha.end(), [](unsigned char ch) { return !isspace(ch); }));
        linha.erase(find_if(linha.rbegin(), linha.rend(), [](unsigned char ch) { return !isspace(ch); }).base(), linha.end());

        if (linha.empty()) {
            continue;
        }

        // Permite separacao explicita de variaveis se o grupo adotar a convencao .data / .text
        if (linha == ".data" || linha == ".DATA") {
            inDataSection = true;
            continue;
        } else if (linha == ".text" || linha == ".TEXT") {
            inDataSection = false;
            continue;
        }

        // Diretivas opcionais de escalonamento (.priority N e .burst N)
        if (linha[0] == '.') {
            stringstream ssDiretiva(linha);
            string diretiva;
            ssDiretiva >> diretiva;

            if (diretiva == ".priority" || diretiva == ".PRIORITY") {
                int valorPrioridade = 0;
                if (!(ssDiretiva >> valorPrioridade)) {
                    cerr << "[JobParser] Valor invalido para .priority no arquivo " << caminho << ": " << linha << endl;
                    return false;
                }
                outJob.prioridade = valorPrioridade;
                continue;
            } else if (diretiva == ".burst" || diretiva == ".BURST") {
                long long valorBurst = 0;
                if (!(ssDiretiva >> valorBurst) || valorBurst < 0) {
                    cerr << "[JobParser] Valor invalido para .burst no arquivo " << caminho << ": " << linha << endl;
                    return false;
                }
                outJob.burst = static_cast<uint32_t>(valorBurst);
                burstDefinido = true;
                continue;
            }
        }

        if (!inDataSection) {
            // Le a instrucao (esperado um valor em Hexadecimal)
            try {
                uint16_t inst = static_cast<uint16_t>(stoul(linha, nullptr, 16));
                outJob.instrucoes.push_back(inst);
            } catch (const exception& e) {
                cerr << "[JobParser] Erro ao converter instrucao no arquivo " << caminho << ": " << linha << endl;
                return false;
            }
        } else {
            // Para as variaveis, assumimos o formato "endereco_relativo valor" ou apenas "valor"
            stringstream ss(linha);
            string part1, part2;
            ss >> part1;
            
            try {
                if (ss >> part2) {
                    // Possui endereco e valor
                    uint16_t addr = static_cast<uint16_t>(stoul(part1, nullptr, 16));
                    uint16_t val  = static_cast<uint16_t>(stoul(part2, nullptr, 16));
                    outJob.variaveis.push_back({addr, val});
                } else {
                    // Possui so o valor: aloca no espaco seguro apos as instrucoes
                    // para evitar colisao com a memoria de codigo em 512
                    uint16_t val  = static_cast<uint16_t>(stoul(part1, nullptr, 16));
                    uint16_t addr = static_cast<uint16_t>(outJob.instrucoes.size() + outJob.variaveis.size());
                    outJob.variaveis.push_back({addr, val});
                }
            } catch (const exception& e) {
                cerr << "[JobParser] Erro ao converter variavel no arquivo " << caminho << ": " << linha << endl;
                return false;
            }
        }
    }
    
    arquivo.close();

    if (!burstDefinido) {
        outJob.burst = static_cast<uint32_t>(outJob.instrucoes.size());
    }

    return true;
}

int JobParser::lerDisco(uint16_t dadoBuscado, const vector<uint16_t>& disco) {
    // Simula a busca sequencial no disco
    // Cada setor lido adiciona uma penalidade de 50 ciclos de clock
    int setoresPercorridos = 0;
    
    for (uint16_t dado : disco) {
        setoresPercorridos++;
        if (dado == dadoBuscado) {
            break; // Dado encontrado
        }
    }
    
    // Registra a latência e o acesso no motor de relógio e na telemetria
    ClockCore::tickDisco(setoresPercorridos);
    MetricsTracker::registrarAcessoDisco();

    // Retorna o total de ciclos gastos na operacao
    return setoresPercorridos * 50;
}
