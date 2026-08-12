#include "cli/cli.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    CLI cli;

    if (argc == 1) {
        cli.runInteractive();
        return 0;
    }

    if (argc == 2) {
        cli.runBatchMode(argv[1]);
        return 0;
    }

    std::cerr << "Usage: " << argv[0] << " [script.txt]" << std::endl;
    return 1;
}
