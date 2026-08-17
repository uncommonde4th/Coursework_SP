#ifndef CLI_HPP
#define CLI_HPP

#include <string>
#include <vector>
#include "parser/parser.hpp"
#include "parser/tokenizer.hpp"
#include "storage_engine.hpp"

class CLI {
public:
    CLI();
    ~CLI();

    void runInteractive();
    void runBatchMode(const std::string& filename);

private:
    std::string readCommand();
    void processCommand(const std::string& command);
    bool isExitCommand(const std::string& command);

    // Задание 7
    void logQuery(const std::string& query, const std::string& status,
                  std::chrono::system_clock::time_point start,
                  std::chrono::system_clock::time_point end);

    // Задание 8
    void printMetrics();

    std::string current_database_;
    sysdb::StorageStub storage_;
    sysdb::Parser parser_;
};

#endif // CLI_HPP