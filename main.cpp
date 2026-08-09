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

    // Подготовка
    parser.parse(Tokenizer("CREATE DATABASE test_db;").tokenize());
    parser.parse(Tokenizer("USE test_db;").tokenize());
    parser.parse(Tokenizer("CREATE TABLE users (id INT NOT_NULL INDEXED, name STRING, age INT);").tokenize());
    parser.parse(Tokenizer("INSERT INTO users VALUE (1, \"Alice\", 25), (2, \"Bob\", 30);").tokenize());

    // Тест DROP TABLE
    std::cout << "\n--- Test DROP TABLE ---" << std::endl;
    parser.parse(Tokenizer("DROP TABLE users;").tokenize());
    if (parser.hasError()) std::cerr << "Error: " << parser.getError() << std::endl;

    // Тест DELETE
    std::cout << "\n--- Test DELETE ---" << std::endl;
    // Сначала пересоздадим таблицу, так как мы её удалили
    parser.parse(Tokenizer("CREATE TABLE users (id INT NOT_NULL INDEXED, name STRING, age INT);").tokenize());

    std::string del_q = "DELETE FROM users WHERE id == 1;";
    std::cout << "Query: " << del_q << std::endl;
    parser.parse(Tokenizer(del_q).tokenize());
    if (parser.hasError()) std::cerr << "Error: " << parser.getError() << std::endl;

    // Тест UPDATE
    std::cout << "\n--- Test UPDATE ---" << std::endl;
    std::string upd_q = "UPDATE users SET age = 26, name = \"Alice Updated\" WHERE id == 1;";
    std::cout << "Query: " << upd_q << std::endl;
    parser.parse(Tokenizer(upd_q).tokenize());
    if (parser.hasError()) std::cerr << "Error: " << parser.getError() << std::endl;

    std::cout << "\n--- Test SELECT ---" << std::endl;
    std::string sel_q = "SELECT id, name AS user_name, age FROM users;";
    std::cout << "Query: " << sel_q << std::endl;
    parser.parse(Tokenizer(sel_q).tokenize());
    if (parser.hasError()) std::cerr << "Error: " << parser.getError() << std::endl;

    std::string sel_star = "SELECT * FROM users;";
    std::cout << "Query: " << sel_star << std::endl;
    parser.parse(Tokenizer(sel_star).tokenize());
    if (parser.hasError()) std::cerr << "Error: " << parser.getError() << std::endl;

    return 0;
}