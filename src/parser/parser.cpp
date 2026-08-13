#include "parser/parser.hpp"
#include <algorithm>
#include <cctype>

namespace sysdb {

Parser::Parser(StorageStub& storage) : storage_(storage), current_pos_(0) {}

Token Parser::peek() {
    if (current_pos_ < tokens_.size()) return tokens_[current_pos_];
    return Token(TokenType::END_OF_FILE, "", 0);
}

Token Parser::consume() {
    if (current_pos_ < tokens_.size()) return tokens_[current_pos_++];
    return Token(TokenType::END_OF_FILE, "", 0);
}

bool Parser::match(TokenType type) {
    if (peek().type == type) { consume(); return true; }
    return false;
}

void Parser::expect(TokenType type) {
    if (!match(type)) {
        error_ = "Syntax Error: Expected '" + tokenTypeToString(type) +
                 "' but found '" + tokenTypeToString(peek().type) +
                 "' at position " + std::to_string(peek().position);
    }
}

bool isKeyword(const Token& t, const std::string& keyword) {
    std::string val = t.value;
    std::transform(val.begin(), val.end(), val.begin(), ::toupper);
    return val == keyword;
}

// ============================================================
// Рекурсивный спуск для условий (Задание 11)
// ============================================================
ConditionPtr Parser::parseOrExpr() {
    auto left = parseAndExpr();
    if (!left) return nullptr;
    while (isKeyword(peek(), "OR")) {
        consume();
        auto right = parseAndExpr();
        if (!right) return nullptr;
        left = ConditionNode::makeLogical(LogicOp::OR, left, right);
    }
    return left;
}

ConditionPtr Parser::parseAndExpr() {
    auto left = parseAtom();
    if (!left) return nullptr;
    while (isKeyword(peek(), "AND")) {
        consume();
        auto right = parseAtom();
        if (!right) return nullptr;
        left = ConditionNode::makeLogical(LogicOp::AND, left, right);
    }
    return left;
}

ConditionPtr Parser::parseAtom() {
    if (match(TokenType::LPAREN)) {
        auto expr = parseOrExpr();
        if (!expr) return nullptr;
        expect(TokenType::RPAREN);
        if (!error_.empty()) return nullptr;
        return expr;
    }

    Operand left;
    if (!parseOperand(left)) return nullptr;

    Token op = peek();
    if (isKeyword(op, "BETWEEN")) {
        consume();
        Operand low, high;
        if (!parseOperand(low)) return nullptr;
        if (!isKeyword(peek(), "AND")) { error_ = "Syntax Error: Expected AND in BETWEEN expression"; return nullptr; }
        consume();
        if (!parseOperand(high)) return nullptr;
        return ConditionNode::makeComparison(left, CondOp::BETWEEN, low, high);
    }
    if (isKeyword(op, "LIKE")) {
        consume();
        Operand pattern;
        if (!parseOperand(pattern)) return nullptr;
        return ConditionNode::makeComparison(left, CondOp::LIKE, pattern);
    }

    CondOp condOp;
    if (match(TokenType::OP_EQ))       condOp = CondOp::EQ;
    else if (match(TokenType::OP_NEQ)) condOp = CondOp::NEQ;
    else if (match(TokenType::OP_LT))  condOp = CondOp::LT;
    else if (match(TokenType::OP_GT))  condOp = CondOp::GT;
    else if (match(TokenType::OP_LTE)) condOp = CondOp::LTE;
    else if (match(TokenType::OP_GTE)) condOp = CondOp::GTE;
    else { error_ = "Syntax Error: Expected comparison operator at position " + std::to_string(peek().position); return nullptr; }

    Operand right;
    if (!parseOperand(right)) return nullptr;
    return ConditionNode::makeComparison(left, condOp, right);
}

// ============================================================
// Основной парсер
// ============================================================
CommandPtr Parser::parse(const std::vector<Token>& tokens) {
    tokens_ = tokens;
    current_pos_ = 0;
    error_.clear();
    if (tokens_.empty()) return nullptr;

    auto toUpper = [](const std::string& s) {
        std::string res = s;
        std::transform(res.begin(), res.end(), res.begin(), ::toupper);
        return res;
    };
    std::string val = toUpper(tokens_[0].value);

    if (val == "CREATE") {
        consume();
        std::string sv = toUpper(peek().value);
        if (sv == "DATABASE") return parseCreateDatabase();
        if (sv == "TABLE") return parseCreateTable();
        error_ = "Syntax Error: Expected DATABASE or TABLE after CREATE";
    }
    else if (val == "DROP") {
        consume();
        std::string sv = toUpper(peek().value);
        if (sv == "DATABASE") return parseDropDatabase();
        if (sv == "TABLE") return parseDropTable();
        error_ = "Syntax Error: Expected DATABASE or TABLE after DROP";
    }
    else if (val == "DELETE") { consume(); return parseDelete(); }
    else if (val == "USE")     { return parseUseDatabase(); }
    else if (val == "INSERT")  { consume(); return parseInsert(); }
    else if (val == "UPDATE")  { consume(); return parseUpdate(); }
    else if (val == "SELECT")  { consume(); return parseSelect(); }
    else { error_ = "Syntax Error: Unknown command '" + tokens_[0].value + "'"; }
    return nullptr;
}

CommandPtr Parser::parseCreateDatabase() {
    expect(TokenType::KW_DATABASE);
    if (!error_.empty()) {
        if (error_.find("Expected") != std::string::npos && isKeyword(peek(), "DATABASE")) { consume(); error_.clear(); }
        else return nullptr;
    }
    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected database name after CREATE DATABASE"; return nullptr; }
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;
    if (storage_.databaseExists(nameToken.value)) { error_ = "Semantic Error: Database '" + nameToken.value + "' already exists"; return nullptr; }
    auto cmd = std::make_unique<CreateDatabaseCmd>();
    cmd->name = nameToken.value;
    storage_.createDatabase(cmd->name);
    if (storage_.hasError()) return nullptr;
    return cmd;
}

CommandPtr Parser::parseDropDatabase() {
    expect(TokenType::KW_DATABASE);
    if (!error_.empty()) {
        if (error_.find("Expected") != std::string::npos && isKeyword(peek(), "DATABASE")) { consume(); error_.clear(); }
        else return nullptr;
    }
    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected database name after DROP DATABASE"; return nullptr; }
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;
    if (!storage_.databaseExists(nameToken.value)) { error_ = "Semantic Error: Database '" + nameToken.value + "' does not exist"; return nullptr; }
    auto cmd = std::make_unique<DropDatabaseCmd>();
    cmd->name = nameToken.value;
    storage_.dropDatabase(cmd->name);
    if (storage_.hasError()) return nullptr;
    return cmd;
}

CommandPtr Parser::parseUseDatabase() {
    consume();
    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected database name after USE"; return nullptr; }
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;
    if (!storage_.databaseExists(nameToken.value)) { error_ = "Semantic Error: Database '" + nameToken.value + "' does not exist"; return nullptr; }
    auto cmd = std::make_unique<UseDatabaseCmd>();
    cmd->name = nameToken.value;
    storage_.useDatabase(cmd->name);
    if (storage_.hasError()) return nullptr;
    return cmd;
}

CommandPtr Parser::parseCreateTable() {
    consume();
    Token nameToken = consume();
    if (nameToken.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected table name after CREATE TABLE"; return nullptr; }
    expect(TokenType::LPAREN);
    if (!error_.empty()) return nullptr;
    std::vector<ColumnDef> columns;
    while (peek().type != TokenType::RPAREN && peek().type != TokenType::END_OF_FILE) {
        Token colName = consume();
        if (colName.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected column name"; return nullptr; }
        Token typeToken = consume();
        if (!isKeyword(typeToken, "INT") && !isKeyword(typeToken, "STRING")) {
            error_ = "Syntax Error: Expected INT or STRING type for column '" + colName.value + "'"; return nullptr;
        }
        ColumnDef col;
        col.name = colName.value;
        col.type = typeToken.value;
        while (isKeyword(peek(), "NOT_NULL") || isKeyword(peek(), "INDEXED")) {
            Token mod = consume();
            if (isKeyword(mod, "NOT_NULL")) col.not_null = true;
            if (isKeyword(mod, "INDEXED")) col.indexed = true;
        }
        columns.push_back(col);
        if (peek().type == TokenType::COMMA) consume(); else break;
    }
    expect(TokenType::RPAREN);
    if (!error_.empty()) return nullptr;
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;
    std::string db = storage_.getCurrentDatabase();
    if (db.empty()) { error_ = "Semantic Error: No database selected. Use USE [database]; first."; return nullptr; }
    if (storage_.tableExists(db, nameToken.value)) {
        error_ = "Semantic Error: Table '" + nameToken.value + "' already exists in database '" + db + "'"; return nullptr;
    }
    auto cmd = std::make_unique<CreateTableCmd>();
    cmd->table_name = nameToken.value;
    cmd->columns = columns;
    storage_.createTable(db, cmd->table_name, cmd->columns);
    if (storage_.hasError()) return nullptr;
    return cmd;
}

CommandPtr Parser::parseInsert() {
    expect(TokenType::KW_INTO);
    if (!error_.empty()) return nullptr;
    std::string db, table;
    if (!parseTableReference(db, table)) { error_ = "Syntax Error: Expected table name after INSERT INTO"; return nullptr; }
    std::vector<std::string> colNames;
    if (peek().type == TokenType::LPAREN) {
        consume();
        while (peek().type != TokenType::RPAREN) {
            Token col = consume();
            if (col.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected column name in list"; return nullptr; }
            colNames.push_back(col.value);
            if (peek().type == TokenType::COMMA) consume();
        }
        expect(TokenType::RPAREN);
        if (!error_.empty()) return nullptr;
    }
    Token valToken = consume();
    if (!isKeyword(valToken, "VALUE")) { error_ = "Syntax Error: Expected VALUE keyword"; return nullptr; }
    std::vector<std::vector<Value>> rows;
    while (peek().type == TokenType::LPAREN) {
        consume();
        std::vector<Value> currentRow;
        while (peek().type != TokenType::RPAREN) {
            Token t = consume();
            Value v;
            if (t.type == TokenType::NUMBER) v = Value::make_int(std::stoll(t.value));
            else if (t.type == TokenType::STRING_LITERAL) v = Value::make_string(t.value);
            else if (isKeyword(t, "NULL")) v = Value::make_null();
            else { error_ = "Syntax Error: Invalid value in INSERT statement"; return nullptr; }
            currentRow.push_back(v);
            if (peek().type == TokenType::COMMA) consume();
        }
        expect(TokenType::RPAREN);
        if (!error_.empty()) return nullptr;
        rows.push_back(currentRow);
        if (peek().type == TokenType::COMMA) consume(); else break;
    }
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;
    if (db.empty()) db = storage_.getCurrentDatabase();
    if (db.empty()) { error_ = "Semantic Error: No database selected."; return nullptr; }
    if (!storage_.tableExists(db, table)) { error_ = "Semantic Error: Table '" + table + "' does not exist"; return nullptr; }
    auto cmd = std::make_unique<InsertCmd>();
    cmd->table_name = table;
    cmd->column_names = colNames;
    cmd->rows = rows;
    if (!storage_.insertRows(db, cmd->table_name, cmd->rows, cmd->column_names)) return nullptr;
    return cmd;
}

CommandPtr Parser::parseDropTable() {
    consume();
    std::string db, table;
    if (!parseTableReference(db, table)) { error_ = "Syntax Error: Expected table name after DROP TABLE"; return nullptr; }
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;
    if (db.empty()) db = storage_.getCurrentDatabase();
    if (db.empty()) { error_ = "Semantic Error: No database selected."; return nullptr; }
    if (!storage_.tableExists(db, table)) { error_ = "Semantic Error: Table '" + table + "' does not exist"; return nullptr; }
    auto cmd = std::make_unique<DropTableCmd>();
    cmd->table_name = table;
    storage_.dropTable(db, cmd->table_name);
    if (storage_.hasError()) return nullptr;
    return cmd;
}

CommandPtr Parser::parseDelete() {
    expect(TokenType::KW_FROM);
    if (!error_.empty()) return nullptr;
    std::string db, table;
    if (!parseTableReference(db, table)) return nullptr;
    Condition cond;
    bool has_where = false;
    if (isKeyword(peek(), "WHERE")) {
        consume();
        has_where = true;
        cond.root = parseOrExpr();
        if (!cond.root) return nullptr;
    }
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;
    if (db.empty()) db = storage_.getCurrentDatabase();
    if (db.empty()) { error_ = "Semantic Error: No database selected."; return nullptr; }
    if (!storage_.tableExists(db, table)) { error_ = "Semantic Error: Table '" + table + "' does not exist"; return nullptr; }
    auto cmd = std::make_unique<DeleteCmd>();
    cmd->table_name = table;
    cmd->has_where = has_where;
    cmd->where = cond;
    if (!storage_.deleteRows(db, table, cond, has_where)) return nullptr;
    return cmd;
}

CommandPtr Parser::parseUpdate() {
    std::string db, table;
    if (!parseTableReference(db, table)) return nullptr;
    expect(TokenType::KW_SET);
    if (!error_.empty()) return nullptr;
    std::vector<std::pair<std::string, Value>> set_clause;
    while (true) {
        Token colToken = consume();
        if (colToken.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected column name in SET clause"; return nullptr; }
        expect(TokenType::ASSIGN);
        if (!error_.empty()) return nullptr;
        Operand operand;
        if (!parseOperand(operand)) return nullptr;
        if (operand.is_column) { error_ = "Syntax Error: SET value must be a constant"; return nullptr; }
        set_clause.push_back({colToken.value, operand.value});
        if (peek().type == TokenType::COMMA) consume(); else break;
    }
    Condition cond;
    bool has_where = false;
    if (isKeyword(peek(), "WHERE")) {
        consume();
        has_where = true;
        cond.root = parseOrExpr();
        if (!cond.root) return nullptr;
    }
    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;
    if (db.empty()) db = storage_.getCurrentDatabase();
    if (db.empty()) { error_ = "Semantic Error: No database selected."; return nullptr; }
    if (!storage_.tableExists(db, table)) { error_ = "Semantic Error: Table '" + table + "' does not exist"; return nullptr; }
    auto cmd = std::make_unique<UpdateCmd>();
    cmd->table_name = table;
    cmd->set_clause = set_clause;
    cmd->has_where = has_where;
    cmd->where = cond;
    if (!storage_.updateRows(db, table, set_clause, cond, has_where)) return nullptr;
    return cmd;
}

// ============================================================
// Задание 12: Парсинг агрегатных функций в SELECT
// ============================================================
CommandPtr Parser::parseSelect() {
    auto cmd = std::make_unique<SelectCmd>();
    std::vector<SelectColumn> cols;

    if (peek().type == TokenType::ASTERISK) {
        consume();
        cols.push_back({"*", "", true, AggFunc::NONE});
    } else {
        while (true) {
            SelectColumn col;
            Token tok = peek();

            // Проверяем агрегатные функции: SUM(...), COUNT(...), AVG(...)
            if (tok.type == TokenType::IDENTIFIER) {
                std::string upper;
                {
                    upper = tok.value;
                    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
                }

                if ((upper == "SUM" || upper == "COUNT" || upper == "AVG") &&
                    current_pos_ + 1 < tokens_.size() && tokens_[current_pos_ + 1].type == TokenType::LPAREN) {

                    consume(); // имя функции
                    consume(); // (

                    if (upper == "SUM") col.agg = AggFunc::SUM;
                    else if (upper == "COUNT") col.agg = AggFunc::COUNT;
                    else col.agg = AggFunc::AVG;

                    // COUNT(*) — специальный случай
                    if (col.agg == AggFunc::COUNT && peek().type == TokenType::ASTERISK) {
                        consume(); // *
                        col.name = "*";
                    } else {
                        Token argToken = consume();
                        if (argToken.type != TokenType::IDENTIFIER) {
                            error_ = "Syntax Error: Expected column name as argument to " + upper;
                            return nullptr;
                        }
                        col.name = argToken.value;
                    }

                    expect(TokenType::RPAREN);
                    if (!error_.empty()) return nullptr;
                } else {
                    // Обычная колонка
                    consume();
                    col.name = tok.value;
                    col.agg = AggFunc::NONE;
                }
            } else {
                error_ = "Syntax Error: Expected column name or aggregate function after SELECT";
                return nullptr;
            }

            // Алиас AS
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
            if (peek().type == TokenType::COMMA) consume();
            else break;
        }
    }

    expect(TokenType::KW_FROM);
    if (!error_.empty()) return nullptr;

    std::string db, table;
    if (!parseTableReference(db, table)) return nullptr;
    cmd->table_name = table;
    cmd->columns = cols;

    if (isKeyword(peek(), "WHERE")) {
        consume();
        cmd->has_where = true;
        cmd->where.root = parseOrExpr();
        if (!cmd->where.root) return nullptr;
    }

    expect(TokenType::SEMICOLON);
    if (!error_.empty()) return nullptr;

    if (db.empty()) db = storage_.getCurrentDatabase();
    if (db.empty()) { error_ = "Semantic Error: No database selected."; return nullptr; }
    if (!storage_.tableExists(db, table)) {
        error_ = "Semantic Error: Table '" + table + "' does not exist"; return nullptr;
    }

    if (!storage_.selectRows(db, table, cols, cmd->where, cmd->has_where)) return nullptr;
    return cmd;
}

bool Parser::parseTableReference(std::string& database, std::string& table) {
    Token name = consume();
    if (name.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected table name"; return false; }
    if (peek().type == TokenType::DOT) {
        consume();
        Token tableToken = consume();
        if (tableToken.type != TokenType::IDENTIFIER) { error_ = "Syntax Error: Expected table name after '.'"; return false; }
        database = name.value;
        table = tableToken.value;
    } else {
        table = name.value;
    }
    return true;
}

bool Parser::parseOperand(Operand& operand) {
    Token token = consume();
    if (token.type == TokenType::IDENTIFIER) {
        operand.is_column = true;
        operand.column = token.value;
        return true;
    }
    if (token.type == TokenType::NUMBER) {
        try { operand.value = Value::make_int(std::stoll(token.value)); }
        catch (...) { error_ = "Syntax Error: Invalid integer literal"; return false; }
        return true;
    }
    if (token.type == TokenType::STRING_LITERAL) {
        operand.value = Value::make_string(token.value);
        return true;
    }
    if (isKeyword(token, "NULL")) {
        operand.value = Value::make_null();
        return true;
    }
    error_ = "Syntax Error: Expected column or constant";
    return false;
}

} // namespace sysdb