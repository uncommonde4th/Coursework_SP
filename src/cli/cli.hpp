#ifndef CLI_HPP
#define CLI_HPP

#include <string>
#include <vector>
#include "parser/parser.hpp"
#include "parser/tokenizer.hpp"
#include "storage_engine.hpp"

class CLI {
public:
    // Конструктор
    CLI();

    // Запуск интерактивного режима
    void runInteractive();

    // Запуск пакетного режима (чтение из файла)
    void runBatchMode(const std::string& filename);

private:
    // Чтение одной команды (может быть многострочной)
    std::string readCommand();

    // Обработка команды (пока просто echo)
    void processCommand(const std::string& command);

    // Проверка, является ли команда командой выхода
    bool isExitCommand(const std::string& command);

    // Флаг текущего контекста (для USE database)
    std::string current_database_;
    sysdb::StorageStub storage_;
    sysdb::Parser parser_;
};

#endif // CLI_HPP
