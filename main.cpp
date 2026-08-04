#include <iostream>
#include <string>
#include "parser/tokenizer.hpp"
#include "parser/parser.hpp"
#include "parser/commands.hpp"
#include "storage_stub.hpp"

using namespace sysdb;

int main() {
    StorageStub storage;
    Parser parser(storage);

    // 1. Подготовка
    parser.parse(Tokenizer("CREATE DATABASE test_db;").tokenize());
    parser.parse(Tokenizer("USE test_db;").tokenize());
    parser.parse(Tokenizer("CREATE TABLE users (id INT NOT_NULL INDEXED, name STRING, age INT);").tokenize());

    // 2. Тест INSERT
    std::string q = "INSERT INTO users VALUE (1, \"Alice\", 25), (2, \"Bob\", 30);";
    std::cout << "\nQuery: " << q << std::endl;

    auto cmd = parser.parse(Tokenizer(q).tokenize());

    if (parser.hasError()) {
        std::cerr << "Error: " << parser.getError() << std::endl;
    } else if (cmd) {
        // Проверяем тип команды, чтобы вывести правильное сообщение
        if (cmd->getType() == CommandType::INSERT) {
            std::cout << "Success! Parsed INSERT command." << std::endl;
        }
    }

    // 3. Тест ошибки (нарушение NOT_NULL)
    std::string q_err = "INSERT INTO users VALUE (NULL, \"Charlie\", 20);";
    std::cout << "\nQuery (should fail): " << q_err << std::endl;

    auto cmd_err = parser.parse(Tokenizer(q_err).tokenize());

    if (parser.hasError()) {
        std::cerr << "Caught Error: " << parser.getError() << std::endl;
    } else if (!cmd_err) {
        std::cerr << "Caught Error: Command returned null without error message." << std::endl;
    }
    return 0;
}