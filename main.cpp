#include <iostream>
#include <string>
#include "cli/cli.hpp"

int main(int argc, char* argv[]) {
    CLI cli;

    if (argc == 1) {
        // Интерактивный режим (без аргументов)
        cli.runInteractive();
    } else if (argc == 2) {
        // Пакетный режим (один аргумент - имя файла)
        std::string filename = argv[1];
        cli.runBatchMode(filename);
    } else {
        // Ошибка: слишком много аргументов
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  Interactive mode: ./sysdb" << std::endl;
        std::cerr << "  Batch mode:       ./sysdb <script_file>" << std::endl;
        return 1;
    }
    
    return 0;
}