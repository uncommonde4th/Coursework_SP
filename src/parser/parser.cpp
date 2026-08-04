#include "parser/parser.hpp"
#include <algorithm>
#include <cctype>

namespace sysdb {

Parser::Parser(StorageStub& storage) : storage_(storage), current_pos_(0) {}

Token Parser::peek() {
    if (current_pos_ < tokens_.size()) {
        return tokens_[current_pos_];
    }
    return Token(TokenType::END_OF_FILE, "", 0);
}

Token Parser::consume() {
    if (current_pos_ < tokens_.size()) {
        return tokens_[current_pos_++];
    }
    return Token(TokenType::END_OF_FILE, "", 0);
}

bool Parser::match(TokenType type) {
    if (peek().type == type) {
        consume();
        return true;
    }
    return false;
}

void Parser::expect(TokenType type) {
    if (!match(type)) {
        error_ = "Syntax Error: Expected '" + tokenTypeToString(type) +
                 "' but found '" + tokenTypeToString(peek().type) +
                 "' at position " + std::to_string(peek().position);
    }
}

bool Parser::checkKeyword(const std::string& keyword) {
    Token t = peek();
    if (t.type == TokenType::IDENTIFIER || t.type >= TokenType::KW_CREATE) { // Проверка на ключевые слова
        std::string upper_val = t.value;
        std::transform(upper_val.begin(), upper_val.end(), upper_val.begin(), ::toupper);
        return upper_val == keyword;
    }
    return false;
}

CommandPtr Parser::parse(const std::vector<Token>& tokens) {
    tokens_ = tokens;
    current_pos_ = 0;
    error_.clear();

    if (tokens_.empty()) {
        return nullptr;
    }

    // Определяем тип команды по первому токену
    if (checkKeyword("CREATE")) {
        consume(); // Пропускаем CREATE
        if (checkKeyword("DATABASE")) {
            return parseCreateDatabase();
        }
        // Здесь позже будет TABLE
    } else if (checkKeyword("DROP")) {
        consume();
        if (checkKeyword("DATABASE")) {
            return parseDropDatabase();
        }
    } else if (checkKeyword("USE")) {
        return parseUseDatabase();
    }

    if (error_.empty()) {
        error_ = "Syntax Error: Unknown command or unsupported syntax at position " +
                 std::to_string(peek().position);
    }

    return nullptr;
}

CommandPtr Parser::parseCreateDatabase() {
    expect(TokenType::KW_DATABASE);
    if (!error_.empty()) return nullptr;

    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected database name (identifier) after CREATE DATABASE";
        return nullptr;
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    // Семантическая проверка
    if (storage_.databaseExists(nameToken.value)) {
        error_ = "Semantic Error: Database '" + nameToken.value + "' already exists";
        return nullptr;
    }

    auto cmd = std::make_unique<CreateDatabaseCmd>();
    cmd->name = nameToken.value;
    storage_.createDatabase(cmd->name);
    return cmd;
}

CommandPtr Parser::parseDropDatabase() {
    expect(TokenType::KW_DATABASE);
    if (!error_.empty()) return nullptr;

    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected database name after DROP DATABASE";
        return nullptr;
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    if (!storage_.databaseExists(nameToken.value)) {
        error_ = "Semantic Error: Database '" + nameToken.value + "' does not exist";
        return nullptr;
    }

    auto cmd = std::make_unique<DropDatabaseCmd>();
    cmd->name = nameToken.value;
    storage_.dropDatabase(cmd->name);
    return cmd;
}

CommandPtr Parser::parseUseDatabase() {
    consume(); // Пропускаем USE

    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected database name after USE";
        return nullptr;
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    if (!storage_.databaseExists(nameToken.value)) {
        error_ = "Semantic Error: Database '" + nameToken.value + "' does not exist";
        return nullptr;
    }

    auto cmd = std::make_unique<UseDatabaseCmd>();
    cmd->name = nameToken.value;
    storage_.useDatabase(cmd->name);
    return cmd;
}

} // namespace sysdb