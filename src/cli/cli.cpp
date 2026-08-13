#include "cli/cli.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include "parser/tokenizer.hpp"
#include "core/utils/access_logger.hpp" // === Задание 7 ===

CLI::CLI() : current_database_(""), storage_(), parser_(storage_) {
    // === Задание 7: Инициализация логгера ===
    if (!sysdb::AccessLogger::instance().initialize("sysdb_data/access.log")) {
        std::cerr << "[WARN] Failed to initialize access log" << std::endl;
    }
    // ========================================
}

// === Задание 7: Корректное завершение логгера ===
CLI::~CLI() {
    sysdb::AccessLogger::instance().shutdown();
}
// ==================================================

void CLI::runInteractive() {
    // === Задание 7 ===
    sysdb::AccessLogger::instance().setClientId("interactive");
    // =================

    std::cout << "SysDB Interactive Mode" << std::endl;
    std::cout << "Type 'exit' or 'quit' to exit." << std::endl;
    std::cout << std::endl;

    while (true) {
        std::cout << "sysdb> ";

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << std::endl;
            break;
        }

        if (isExitCommand(line)) {
            std::cout << "Goodbye!" << std::endl;
            break;
        }

        if (line.empty()) continue;

        std::string command = line;

        std::string trimmed = command;
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

        if (!trimmed.empty() && trimmed.back() == ';') {
            processCommand(command);
            continue;
        }

        while (true) {
            std::cout << "... ";
            if (!std::getline(std::cin, line)) {
                std::cout << std::endl;
                break;
            }

            if (isExitCommand(line)) {
                std::cout << "Goodbye!" << std::endl;
                return;
            }

            if (!command.empty()) command += " ";
            command += line;

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
    // === Задание 7 ===
    sysdb::AccessLogger::instance().setClientId(filename);
    // =================

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
        if (line.empty() || line.substr(0, 2) == "--") continue;

        if (!current_command.empty()) current_command += " ";
        current_command += line;

        std::string trimmed = current_command;
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

        if (!trimmed.empty() && trimmed.back() == ';') {
            processCommand(current_command);
            current_command.clear();
        }
    }

    if (!current_command.empty()) {
        std::cerr << "Warning: Incomplete command at end of file" << std::endl;
        processCommand(current_command);
    }

    file.close();
}

void CLI::processCommand(const std::string& command) {
    // === Задание 7: Замер времени выполнения ===
    auto start = std::chrono::system_clock::now();
    std::string status = "OK";
    // ============================================

    try {
        sysdb::Tokenizer tokenizer(command);
        auto tokens = tokenizer.tokenize();
        if (tokenizer.hasError()) {
            status = tokenizer.getError();
            std::cerr << status << std::endl;
        } else {
            parser_.parse(tokens);
            if (parser_.hasError()) {
                status = parser_.getError();
                std::cerr << status << std::endl;
            }
        }
    } catch (const std::exception& e) {
        status = std::string("Exception: ") + e.what();
        std::cerr << "Error: " << e.what() << std::endl;
    } catch (...) {
        status = "Unknown failure";
        std::cerr << "Error: unknown failure" << std::endl;
    }

    // === Задание 7: Отправка записи в лог ===
    auto end = std::chrono::system_clock::now();
    logQuery(command, status, start, end);
    // =========================================
}

bool CLI::isExitCommand(const std::string& command) {
    std::string lower_cmd = command;
    std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);
    lower_cmd.erase(std::remove_if(lower_cmd.begin(), lower_cmd.end(),
                                   [](char c) { return c == ' ' || c == ';' || c == '\t' || c == '\n' || c == '\r'; }),
                    lower_cmd.end());
    return (lower_cmd == "exit" || lower_cmd == "quit");
}

// === Задание 7: Реализация логирования ===
void CLI::logQuery(const std::string& query, const std::string& status,
                   std::chrono::system_clock::time_point start,
                   std::chrono::system_clock::time_point end) {
    std::ostringstream handler_ss;
    handler_ss << std::this_thread::get_id();

    sysdb::LogEntry entry;
    entry.timestamp_start = sysdb::AccessLogger::formatTime(start);
    entry.timestamp_end = sysdb::AccessLogger::formatTime(end);
    entry.client_id = sysdb::AccessLogger::instance().getClientId();
    entry.handler_id = handler_ss.str();
    entry.query_body = query;
    entry.status = status;

    sysdb::AccessLogger::instance().log(std::move(entry));
}
// ==========================================