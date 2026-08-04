#include <iostream>
#include <string>
#include "parser/tokenizer.hpp"
#include "parser/parser.hpp"
#include "storage_stub.hpp"

using namespace sysdb;

int main() {
    StorageStub storage;
    Parser parser(storage);

    std::cout << "=== Parser Test ===" << std::endl;

    // Тест 1: CREATE DATABASE
    std::string query1 = "CREATE DATABASE my_db;";
    std::cout << "\nQuery: " << query1 << std::endl;

    Tokenizer tok1(query1);
    auto tokens1 = tok1.tokenize();
    if (!tok1.hasError()) {
        auto cmd = parser.parse(tokens1);
        if (parser.hasError()) {
            std::cerr << "Error: " << parser.getError() << std::endl;
        } else if (cmd) {
            std::cout << "Success! Parsed CREATE DATABASE for: " << static_cast<CreateDatabaseCmd*>(cmd.get())->name << std::endl;
        }
    }

    // Тест 2: USE несуществующей БД (ошибка семантики)
    std::string query2 = "USE nonexistent_db;";
    std::cout << "\nQuery: " << query2 << std::endl;

    Tokenizer tok2(query2);
    auto tokens2 = tok2.tokenize();
    if (!tok2.hasError()) {
        auto cmd = parser.parse(tokens2);
        if (parser.hasError()) {
            std::cerr << "Error: " << parser.getError() << std::endl;
        }
    }

    return 0;
}