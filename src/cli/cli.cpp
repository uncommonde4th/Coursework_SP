#include "cli/cli.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <iomanip>
#include "parser/tokenizer.hpp"
#include "core/utils/access_logger.hpp"
#include "core/utils/telemetry.hpp"

CLI::CLI() : current_database_(""), storage_(), parser_(storage_) {
    if (!sysdb::AccessLogger::instance().initialize("sysdb_data/access.log")) {
        std::cerr << "[WARN] Failed to initialize access log" << std::endl;
    }
    sysdb::TelemetryCollector::instance().start();
}

CLI::~CLI() {
    sysdb::AccessLogger::instance().shutdown();
    sysdb::TelemetryCollector::instance().stop();
}

static bool startsWithLetter(const std::string& s) {
    if (s.empty()) return false;
    char c = s[0];
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// Считает баланс скобок в строке, игнорируя скобки внутри "строковых
// литералов" в двойных кавычках. Нужно, чтобы отличить "однострочную
// команду без ;" (баланс 0 - можно смело дописывать ;) от ещё не
// законченной многострочной команды вроде "CREATE TABLE users (" (баланс
// 1 - скобка ещё не закрыта, продолжаем накапливать строки).
static int parenBalance(const std::string& s) {
    int depth = 0;
    bool in_string = false;
    for (char c : s) {
        if (c == '"') { in_string = !in_string; continue; }
        if (in_string) continue;
        if (c == '(') ++depth;
        else if (c == ')') --depth;
    }
    return depth;
}

void CLI::runInteractive() {
    sysdb::AccessLogger::instance().setClientId("interactive");

    std::cout << "SysDB Interactive Mode" << std::endl;
    std::cout << "Type 'exit' or 'quit' to exit." << std::endl;
    std::cout << std::endl;

    std::string accumulated_command = "";

    while (true) {
        std::cerr.flush();
        std::cout.flush();

        // Если это начало новой команды — печатаем "sysdb> ", если продолжение многострочного ввода — "... "
        if (accumulated_command.empty()) {
            std::cout << "sysdb> " << std::flush;
        } else {
            std::cout << "... " << std::flush;
        }

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << std::endl;
            break;
        }

        if (isExitCommand(line)) {
            std::cout << "Goodbye!" << std::endl;
            break;
        }

        // Trim текущей строки
        size_t s = line.find_first_not_of(" \t\n\r");
        if (s == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\n\r");
        std::string trimmed = line.substr(s, e - s + 1);

        if (accumulated_command.empty()) {
            accumulated_command = trimmed;
        } else {
            accumulated_command += " " + trimmed;
        }

        // Автодобавление ; если команда начинается с буквы, еще не имеет ';'
        // на конце, И при этом скобки уже сбалансированы. Последнее условие
        // критично: без него многострочный CREATE TABLE (со скобкой,
        // открытой на первой строке и закрытой через несколько строк) обрывался прямо на первой строке
        if (accumulated_command.back() != ';' && startsWithLetter(accumulated_command)
            && parenBalance(accumulated_command) <= 0) {
            // Для интерактивного режима автоматически завершаем однострочные команды
            accumulated_command += ";";
        }

        // Если команда завершена на ';'
        if (accumulated_command.back() == ';') {
            processCommand(accumulated_command);
            accumulated_command.clear(); // Сбрасываем буфер команды

            std::cerr.flush();
            std::cout.flush();
        }
    }
}


void CLI::runBatchMode(const std::string& filename) {
    sysdb::AccessLogger::instance().setClientId(filename);

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

        size_t e = current_command.find_last_not_of(" \t\n\r");
        std::string trimmed = (e != std::string::npos) ? current_command.substr(0, e + 1) : "";

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
    for (const auto& statement : splitStatements(command)) {
        executeSingleStatement(statement);
    }
}

// Разбивает входную строку на подстроки, каждая из которых заканчивается
// на ';', учитывая, что ';' внутри "строкового литерала" не является
// разделителем операторов.
std::vector<std::string> CLI::splitStatements(const std::string& input) {
    std::vector<std::string> result;
    std::string current;
    bool in_string = false;

    for (char c : input) {
        current += c;
        if (c == '"') {
            in_string = !in_string;
        } else if (c == ';' && !in_string) {
            result.push_back(current);
            current.clear();
        }
    }

    // Остаток без завершающего ';' (в норме такого быть не должно, но
    // ничего не теряем - последующий парсер сам сообщит о синтаксической
    // ошибке "нет ';'", если она там реально есть).
    size_t s = current.find_first_not_of(" \t\r\n");
    if (s != std::string::npos) {
        result.push_back(current);
    }

    return result;
}

void CLI::executeSingleStatement(const std::string& command) {
    // Защита от пустых/мусорных команд
    {
        std::string stripped = command;
        stripped.erase(std::remove_if(stripped.begin(), stripped.end(),
            [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';'; }),
            stripped.end());
        if (stripped.empty()) return;
    }

    auto start = std::chrono::system_clock::now();
    std::string status = "OK";

    // Встроенная команда METRICS
    {
        std::string upper_cmd = command;
        std::transform(upper_cmd.begin(), upper_cmd.end(), upper_cmd.begin(), ::toupper);
        upper_cmd.erase(std::remove_if(upper_cmd.begin(), upper_cmd.end(),
            [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }),
            upper_cmd.end());

        if (upper_cmd == "METRICS;") {
            printMetrics();
            return;
        }
    }

    try {
        sysdb::Tokenizer tokenizer(command);
        auto tokens = tokenizer.tokenize();
        if (tokenizer.hasError()) {
            status = tokenizer.getError();
        } else {
            parser_.parse(tokens);
            if (parser_.hasError()) {
                status = parser_.getError();
            }
        }
    } catch (const std::exception& e) {
        status = std::string("Exception: ") + e.what();
    } catch (...) {
        status = "Unknown failure";
    }

    if (status != "OK") {
        // Проверяем, напечатал ли Storage уже что-то в свой error buffer.
        // Если status НЕ начинается с [STORAGE], и Storage ничего сам не печатал — выводим статус от парсера
        bool printedByStorage = (!storage_.getError().empty() && status.find(storage_.getError()) != std::string::npos);

        if (!printedByStorage) {
            std::cerr << status << std::endl;
        } else {
            // Если Storage печатал сам, убеждаемся, что он завершил строку!
            std::cerr << std::endl;
        }
        std::cerr.flush();
        std::cout.flush();
    }
    // ============================

    auto end = std::chrono::system_clock::now();

    logQuery(command, status, start, end);

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    bool is_error = (status != "OK");
    sysdb::TelemetryCollector::instance().recordRequest(latency_ms, is_error);
}

bool CLI::isExitCommand(const std::string& command) {
    std::string lower_cmd = command;
    std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);
    lower_cmd.erase(std::remove_if(lower_cmd.begin(), lower_cmd.end(),
                                   [](char c) { return c == ' ' || c == ';' || c == '\t' || c == '\n' || c == '\r'; }),
                    lower_cmd.end());
    return (lower_cmd == "exit" || lower_cmd == "quit");
}

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

void CLI::printMetrics() {
    auto snap = sysdb::TelemetryCollector::instance().getSnapshot();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "{"
              << "\"current_rps\":" << snap.current_rps << ","
              << "\"avg_rps_10min\":" << snap.avg_rps_10min << ","
              << "\"max_rps_10min\":" << snap.max_rps_10min << ","
              << "\"avg_latency_10sec_ms\":" << snap.avg_latency_10sec << ","
              << "\"error_count_1min\":" << snap.error_count_1min
              << "}" << std::endl;
}