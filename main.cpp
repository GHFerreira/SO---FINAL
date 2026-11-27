#include "src/config.hpp"
#include "src/simulador.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    vector<string> listaJobs;
    Config config;
    string erro;

    if (!Config::analisarArgumentos(argc, argv, config, listaJobs, erro)) {
        cerr << "Erro: " << erro << '\n';
        cerr << "Use --help para consultar as opcoes.\n";
        return EXIT_FAILURE;
    }

    if (config.ajudaSolicitada) {
        cout << Config::textoAjuda();
        return EXIT_SUCCESS;
    }

    if (!config.validarExecucaoFase42(erro)) {
        cerr << "Erro: " << erro << '\n';
        return EXIT_FAILURE;
    }

    if (listaJobs.empty()) {
        listaJobs = {
            "tests/job1_soma.txt",
            "tests/job2_vetor.txt",
            "tests/job3_loop.txt"
        };
    }

    Simulador::executarLote(listaJobs, config);
    return 0;
}
