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

// Вспомогательная функция для безопасного сравнения ключевых слов
bool isKeyword(const Token& t, const std::string& keyword) {
    // Ключевые слова могут быть распознаны как KW_... или как IDENTIFIER, если они не в списке зарезервированных
    // Но у нас все ключевые слова есть в TokenType.
    // Для надежности сравниваем значение токена, приводя к верхнему регистру
    std::string val = t.value;
    std::transform(val.begin(), val.end(), val.begin(), ::toupper);
    return val == keyword;
}

    CommandPtr Parser::parse(const std::vector<Token>& tokens) {
    tokens_ = tokens;
    current_pos_ = 0;
    error_.clear();

    if (tokens_.empty()) {
        return nullptr;
    }

    Token first = peek();

    // Приводим к верхнему регистру для универсального сравнения
    auto toUpper = [](const std::string& s) {
        std::string res = s;
        std::transform(res.begin(), res.end(), res.begin(), ::toupper);
        return res;
    };

    std::string val = toUpper(first.value);

    if (val == "CREATE") {
        consume();
        Token second = peek();
        std::string secondVal = toUpper(second.value);

        if (secondVal == "DATABASE") return parseCreateDatabase();
        if (secondVal == "TABLE") return parseCreateTable();

        error_ = "Syntax Error: Expected DATABASE or TABLE after CREATE";
    }
    else if (val == "DROP") {
        consume();
        Token second = peek();
        std::string secondVal = toUpper(second.value);

        if (secondVal == "DATABASE") return parseDropDatabase();

        error_ = "Syntax Error: Expected DATABASE after DROP";
    }
    else if (val == "USE") {
        return parseUseDatabase();
    }
    else if (val == "INSERT") {
        consume();
        return parseInsert();
    }
    else {
        error_ = "Syntax Error: Unknown command '" + first.value + "'";
    }

    return nullptr;
}

CommandPtr Parser::parseCreateDatabase() {
    expect(TokenType::KW_DATABASE); // На случай, если лексер распознал его как KEYWORD
    if (!error_.empty()) {
        // Если не вышло через expect, пробуем просто съесть следующее слово, если это DATABASE
        if (error_.find("Expected") != std::string::npos && isKeyword(peek(), "DATABASE")) {
             consume();
             error_.clear();
        } else {
             return nullptr;
        }
    }

    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected database name after CREATE DATABASE";
        return nullptr;
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

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
     if (!error_.empty()) {
        if (error_.find("Expected") != std::string::npos && isKeyword(peek(), "DATABASE")) {
             consume();
             error_.clear();
        } else {
             return nullptr;
        }
    }

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
    consume(); // Съели USE

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

CommandPtr Parser::parseCreateTable() {
    consume(); // Съели TABLE (так как мы уже проверили его в parse())

    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected table name after CREATE TABLE";
        return nullptr;
    }

    expect(TokenType::LPAREN);
    if (!error_.empty()) return nullptr;

    std::vector<ColumnDef> columns;

    while (peek().type != TokenType::RPAREN && peek().type != TokenType::END_OF_FILE) {
        Token colName = consume();
        if (colName.type != TokenType::IDENTIFIER) {
            error_ = "Syntax Error: Expected column name";
            return nullptr;
        }

        Token typeToken = consume();
        // Проверяем тип, учитывая, что INT и STRING могут быть ключевыми словами
        if (!isKeyword(typeToken, "INT") && !isKeyword(typeToken, "STRING")) {
            error_ = "Syntax Error: Expected INT or STRING type for column '" + colName.value + "'";
            return nullptr;
        }

        ColumnDef col;
        col.name = colName.value;
        col.type = typeToken.value;

        // Проверка модификаторов
        while (isKeyword(peek(), "NOT_NULL") || isKeyword(peek(), "INDEXED")) {
            Token mod = consume();
            if (isKeyword(mod, "NOT_NULL")) col.not_null = true;
            if (isKeyword(mod, "INDEXED")) col.indexed = true;
        }

        columns.push_back(col);

        if (peek().type == TokenType::COMMA) {
            consume();
        } else {
            break;
        }
    }

    expect(TokenType::RPAREN);
    if (!error_.empty()) return nullptr;

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    std::string db = storage_.getCurrentDatabase();
    if (db.empty()) {
        error_ = "Semantic Error: No database selected. Use USE [database]; first.";
        return nullptr;
    }

    if (storage_.tableExists(db, nameToken.value)) {
        error_ = "Semantic Error: Table '" + nameToken.value + "' already exists in database '" + db + "'";
        return nullptr;
    }

    auto cmd = std::make_unique<CreateTableCmd>();
    cmd->table_name = nameToken.value;
    cmd->columns = columns;

    storage_.createTable(db, cmd->table_name, cmd->columns);
    return cmd;
}

CommandPtr Parser::parseInsert() {
    expect(TokenType::KW_INTO); // INTO
    if (!error_.empty()) return nullptr;

    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected table name after INSERT INTO";
        return nullptr;
    }

    std::vector<std::string> colNames;

    // Опциональный список колонок: (col1, col2)
    if (peek().type == TokenType::LPAREN) {
        consume();
        while (peek().type != TokenType::RPAREN) {
            Token col = consume();
            if (col.type != TokenType::IDENTIFIER) {
                error_ = "Syntax Error: Expected column name in list";
                return nullptr;
            }
            colNames.push_back(col.value);
            if (peek().type == TokenType::COMMA) consume();
        }
        expect(TokenType::RPAREN);
        if (!error_.empty()) return nullptr;
    }

    // Ожидаем VALUE
    Token valToken = consume();
    if (!isKeyword(valToken, "VALUE")) {
        error_ = "Syntax Error: Expected VALUE keyword";
        return nullptr;
    }

    std::vector<std::vector<Value>> rows;

    // Парсинг кортежей: (val1, val2), (val3, val4)
    while (peek().type == TokenType::LPAREN) {
        consume(); // (
        std::vector<Value> currentRow;

        while (peek().type != TokenType::RPAREN) {
            Token t = consume();
            Value v;

            if (t.type == TokenType::NUMBER) {
                v = Value::make_int(std::stoll(t.value));
            } else if (t.type == TokenType::STRING_LITERAL) {
                v = Value::make_string(t.value);
            } else if (isKeyword(t, "NULL")) {
                v = Value::make_null();
            } else {
                error_ = "Syntax Error: Invalid value in INSERT statement";
                return nullptr;
            }
            currentRow.push_back(v);

            if (peek().type == TokenType::COMMA) consume();
        }

        expect(TokenType::RPAREN);
        if (!error_.empty()) return nullptr;

        rows.push_back(currentRow);

        // Если следующая запятая, значит есть еще кортеж
        if (peek().type == TokenType::COMMA) {
            consume();
        } else {
            break;
        }
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    // Семантическая проверка и выполнение
    std::string db = storage_.getCurrentDatabase();
    if (db.empty()) {
        error_ = "Semantic Error: No database selected.";
        return nullptr;
    }

    if (!storage_.tableExists(db, nameToken.value)) {
        error_ = "Semantic Error: Table '" + nameToken.value + "' does not exist";
        return nullptr;
    }

    auto cmd = std::make_unique<InsertCmd>();
    cmd->table_name = nameToken.value;
    cmd->column_names = colNames;
    cmd->rows = rows;

    storage_.insertRows(db, cmd->table_name, cmd->rows);
    return cmd;
}

} // namespace sysdb