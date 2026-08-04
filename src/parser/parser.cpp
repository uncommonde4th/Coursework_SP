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
        if (secondVal == "TABLE") return parseDropTable(); // <-- Новая ветка

        error_ = "Syntax Error: Expected DATABASE or TABLE after DROP";
    }
    else if (val == "DELETE") {
        consume();
        return parseDelete(); // <-- Новая ветка
    }
    else if (val == "USE") {
        return parseUseDatabase();
    }
    else if (val == "INSERT") {
        consume();
        return parseInsert();
    }
    else if (val == "UPDATE") {
        consume();
        return parseUpdate();
    }
    else if (val == "SELECT") {
        consume();
        return parseSelect();
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

CommandPtr Parser::parseDropTable() {
    consume(); // Съели TABLE
    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected table name after DROP TABLE";
        return nullptr;
    }
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    std::string db = storage_.getCurrentDatabase();
    if (db.empty()) {
        error_ = "Semantic Error: No database selected.";
        return nullptr;
    }

    if (!storage_.tableExists(db, nameToken.value)) {
        error_ = "Semantic Error: Table '" + nameToken.value + "' does not exist";
        return nullptr;
    }

    auto cmd = std::make_unique<DropTableCmd>();
    cmd->table_name = nameToken.value;
    storage_.dropTable(db, cmd->table_name);
    return cmd;
}

CommandPtr Parser::parseDelete() {
    expect(TokenType::KW_FROM); // FROM
    if (!error_.empty()) return nullptr;

    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected table name after DELETE FROM";
        return nullptr;
    }

    Condition cond;
    bool has_where = false;

    // Проверяем наличие WHERE
    if (isKeyword(peek(), "WHERE")) {
        consume(); // Съели WHERE
        has_where = true;

        Token colToken = consume();
        if (colToken.type != TokenType::IDENTIFIER) {
            error_ = "Syntax Error: Expected column name in WHERE clause";
            return nullptr;
        }
        cond.column = colToken.value;

        Token opToken = consume();
        // Проверяем операторы сравнения
        if (opToken.type >= TokenType::OP_EQ && opToken.type <= TokenType::OP_GTE) {
            cond.op = opToken.value;
        } else {
            error_ = "Syntax Error: Expected comparison operator in WHERE clause";
            return nullptr;
        }

        Token valToken = consume();
        if (valToken.type == TokenType::NUMBER) {
            cond.value = Value::make_int(std::stoll(valToken.value));
        } else if (valToken.type == TokenType::STRING_LITERAL) {
            cond.value = Value::make_string(valToken.value);
        } else {
            error_ = "Syntax Error: Invalid value in WHERE clause";
            return nullptr;
        }
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    std::string db = storage_.getCurrentDatabase();
    if (db.empty()) {
        error_ = "Semantic Error: No database selected.";
        return nullptr;
    }

    if (!storage_.tableExists(db, nameToken.value)) {
        error_ = "Semantic Error: Table '" + nameToken.value + "' does not exist";
        return nullptr;
    }

    auto cmd = std::make_unique<DeleteCmd>();
    cmd->table_name = nameToken.value;
    cmd->has_where = has_where;
    if (has_where) cmd->where = cond;

    if (has_where) {
        storage_.deleteRows(db, cmd->table_name, cmd->where);
    } else {
        std::cout << "[STORAGE] Warning: DELETE without WHERE will remove all rows (not implemented in stub)." << std::endl;
    }

    return cmd;
}

CommandPtr Parser::parseUpdate() {
    Token nameToken = consume(); // Имя таблицы
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected table name after UPDATE";
        return nullptr;
    }

    expect(TokenType::KW_SET); // SET
    if (!error_.empty()) return nullptr;

    std::vector<std::pair<std::string, Value>> set_clause;

    // Парсинг списка присваиваний: col = val, col2 = val2
    while (true) {
        Token colToken = consume();
        if (colToken.type != TokenType::IDENTIFIER) {
            error_ = "Syntax Error: Expected column name in SET clause";
            return nullptr;
        }

        expect(TokenType::ASSIGN); // =
        if (!error_.empty()) return nullptr;

        Token valToken = consume();
        Value v;
        if (valToken.type == TokenType::NUMBER) {
            v = Value::make_int(std::stoll(valToken.value));
        } else if (valToken.type == TokenType::STRING_LITERAL) {
            v = Value::make_string(valToken.value);
        } else if (isKeyword(valToken, "NULL")) {
            v = Value::make_null();
        } else {
            error_ = "Syntax Error: Invalid value in SET clause";
            return nullptr;
        }

        set_clause.push_back({colToken.value, v});

        if (peek().type == TokenType::COMMA) {
            consume();
        } else {
            break;
        }
    }

    Condition cond;
    bool has_where = false;

    // Проверяем наличие WHERE
    if (isKeyword(peek(), "WHERE")) {
        consume();
        has_where = true;

        Token colToken = consume();
        if (colToken.type != TokenType::IDENTIFIER) {
            error_ = "Syntax Error: Expected column name in WHERE clause";
            return nullptr;
        }
        cond.column = colToken.value;

        Token opToken = consume();
        if (opToken.type >= TokenType::OP_EQ && opToken.type <= TokenType::OP_GTE) {
            cond.op = opToken.value;
        } else {
            error_ = "Syntax Error: Expected comparison operator in WHERE clause";
            return nullptr;
        }

        Token valToken = consume();
        if (valToken.type == TokenType::NUMBER) {
            cond.value = Value::make_int(std::stoll(valToken.value));
        } else if (valToken.type == TokenType::STRING_LITERAL) {
            cond.value = Value::make_string(valToken.value);
        } else {
            error_ = "Syntax Error: Invalid value in WHERE clause";
            return nullptr;
        }
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    std::string db = storage_.getCurrentDatabase();
    if (db.empty()) {
        error_ = "Semantic Error: No database selected.";
        return nullptr;
    }

    if (!storage_.tableExists(db, nameToken.value)) {
        error_ = "Semantic Error: Table '" + nameToken.value + "' does not exist";
        return nullptr;
    }

    auto cmd = std::make_unique<UpdateCmd>();
    cmd->table_name = nameToken.value;
    cmd->set_clause = set_clause;
    cmd->has_where = has_where;
    if (has_where) cmd->where = cond;

    storage_.updateRows(db, cmd->table_name, cmd->set_clause, cmd->where);
    return cmd;
}

CommandPtr Parser::parseSelect() {
    auto cmd = std::make_unique<SelectCmd>(); // Создаем команду сразу

    std::vector<SelectColumn> cols;

    // Проверяем, это * или список колонок
    if (peek().type == TokenType::ASTERISK) {
        consume();
        cols.push_back({ "*", "", true });
    } else {
        while (true) {
            Token colToken = consume();
            if (colToken.type != TokenType::IDENTIFIER && colToken.type != TokenType::ASTERISK) {
                error_ = "Syntax Error: Expected column name or * after SELECT";
                return nullptr;
            }

            SelectColumn col;
            col.name = colToken.value;
            col.is_star = (colToken.type == TokenType::ASTERISK);

            // Проверка на алиас (AS)
            if (isKeyword(peek(), "AS")) {
                consume();
                Token aliasToken = consume();
                if (aliasToken.type != TokenType::IDENTIFIER) {
                    error_ = "Syntax Error: Expected alias name after AS";
                    return nullptr;
                }
                col.alias = aliasToken.value;
            }

            cols.push_back(col);

            if (peek().type == TokenType::COMMA) {
                consume();
            } else {
                break;
            }
        }
    }

    expect(TokenType::KW_FROM);
    if (!error_.empty()) return nullptr;

    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) {
        error_ = "Syntax Error: Expected table name after FROM";
        return nullptr;
    }

    cmd->table_name = nameToken.value;
    cmd->columns = cols;

    // Проверяем наличие WHERE
    if (isKeyword(peek(), "WHERE")) {
        consume(); // Съели WHERE
        cmd->has_where = true;

        Token colToken = consume();
        if (colToken.type != TokenType::IDENTIFIER) {
            error_ = "Syntax Error: Expected column name in WHERE clause";
            return nullptr;
        }
        cmd->where.column = colToken.value;

        Token opToken = consume();
        if (opToken.type >= TokenType::OP_EQ && opToken.type <= TokenType::OP_GTE) {
            cmd->where.op = opToken.value;
        } else {
            error_ = "Syntax Error: Expected comparison operator in WHERE clause";
            return nullptr;
        }

        Token valToken = consume();
        if (valToken.type == TokenType::NUMBER) {
            cmd->where.value = Value::make_int(std::stoll(valToken.value));
        } else if (valToken.type == TokenType::STRING_LITERAL) {
            cmd->where.value = Value::make_string(valToken.value);
        } else {
            error_ = "Syntax Error: Invalid value in WHERE clause";
            return nullptr;
        }
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    std::string db = storage_.getCurrentDatabase();
    if (db.empty()) {
        error_ = "Semantic Error: No database selected.";
        return nullptr;
    }

    if (!storage_.tableExists(db, cmd->table_name)) {
        error_ = "Semantic Error: Table '" + cmd->table_name + "' does not exist";
        return nullptr;
    }

    std::cout << "[PARSER] Parsed SELECT for table '" << cmd->table_name << "' with "
              << cmd->columns.size() << " columns." << std::endl;

    return cmd;
}

} // namespace sysdb