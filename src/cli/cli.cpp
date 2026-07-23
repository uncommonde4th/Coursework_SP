#include "cli/cli.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

CLI::CLI() : current_database_("") {}

void CLI::runInteractive() {
    std::cout << "SysDB Interactive Mode" << std::endl;
    std::cout << "Type 'exit' or 'quit' to exit." << std::endl;
    std::cout << std::endl;

    while (true) {
        std::cout << "sysdb> ";

        // Сначала читаем первую строку
        std::string line;
        if (!std::getline(std::cin, line)) {
            // EOF достигнут (Ctrl+D)
            std::cout << std::endl;
            break;
        }

        // Проверяем, не является ли это командой выхода
        if (isExitCommand(line)) {
            std::cout << "Goodbye!" << std::endl;
            break;
        }

        // Если строка пустая, пропускаем
        if (line.empty()) {
            continue;
        }

        // Начинаем накапливать команду
        std::string command = line;

        // Проверяем, заканчивается ли команда на ;
        std::string trimmed = command;
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

        // Если команда завершена, обрабатываем сразу
        if (!trimmed.empty() && trimmed.back() == ';') {
            processCommand(command);
            continue;
        }

        // Иначе читаем продолжение
        while (true) {
            std::cout << "... ";
            if (!std::getline(std::cin, line)) {
                // EOF достигнут
                std::cout << std::endl;
                break;
            }

            // Проверяем на exit/quit в каждой новой строке
            if (isExitCommand(line)) {
                std::cout << "Goodbye!" << std::endl;
                return;
            }

            if (!command.empty()) {
                command += " ";
            }
            command += line;

            // Проверяем завершение команды
            trimmed = command;
            trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

            if (!trimmed.empty() && trimmed.back() == ';') {
                processCommand(command);
                break;
            }
        }
    }
}

void CLI::runBatchMode(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file '" << filename << "'" << std::endl;
        return;
    }

    std::cout << "SysDB Batch Mode - Reading from: " << filename << std::endl;
    std::cout << std::endl;

    std::string line;
    std::string current_command;

    while (std::getline(file, line)) {
        // Пропускаем пустые строки и комментарии (начинаются с --)
        if (line.empty() || line.substr(0, 2) == "--") {
            continue;
        }

        // Добавляем строку к текущей команде
        if (!current_command.empty()) {
            current_command += " ";
        }
        current_command += line;

        // Проверяем, заканчивается ли команда на ;
        // Удаляем пробелы в конце для проверки
        std::string trimmed = current_command;
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

        if (!trimmed.empty() && trimmed.back() == ';') {
            // Команда завершена
            processCommand(current_command);
            current_command.clear();
        }
    }

    // Если осталась незавершённая команда
    if (!current_command.empty()) {
        std::cerr << "Warning: Incomplete command at end of file" << std::endl;
        processCommand(current_command);
    }

    file.close();
}

void CLI::processCommand(const std::string& command) {
    // Пока просто выводим команду обратно (echo)
    std::cout << "[Received]: " << command << std::endl;

    // TODO: Здесь будет парсинг и выполнение команды
}

bool CLI::isExitCommand(const std::string& command) {
    // Приводим к нижнему регистру для сравнения
    std::string lower_cmd = command;
    std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);

    // Удаляем пробелы, точку с запятой и другие символы
    lower_cmd.erase(std::remove_if(lower_cmd.begin(), lower_cmd.end(),
                                   [](char c) { return c == ' ' || c == ';' || c == '\t' || c == '\n' || c == '\r'; }),
                    lower_cmd.end());

    return (lower_cmd == "exit");
}