#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <string>
#include <vector>
#include <iostream>

namespace sysdb {

enum class TokenType {
    // Ключевые слова
    KW_CREATE, KW_DROP, KW_DATABASE, KW_TABLE, KW_USE,
    KW_INSERT, KW_INTO, KW_VALUE, KW_UPDATE, KW_SET, KW_DELETE, KW_FROM,
    KW_SELECT, KW_WHERE, KW_AS, KW_AND, KW_OR, KW_NOT, KW_NULL,
    KW_BETWEEN, KW_LIKE, KW_INT, KW_STRING, KW_NOT_NULL, KW_INDEXED,
    KW_DEFAULT, KW_REVERT,

    // Литералы и идентификаторы
    IDENTIFIER,   // Имена таблиц, колонок, БД
    NUMBER,       // Целые числа
    STRING_LITERAL, // Строки в "кавычках"
    TIMESTAMP_LITERAL, // yyyy.mm.dd-hh:mm:ss.msmsms (для REVERT)

    // Операторы и символы
    OP_EQ,        // ==
    OP_NEQ,       // !=
    OP_LT,        // <
    OP_GT,        // >
    OP_LTE,       // <=
    OP_GTE,       // >=
    ASTERISK,     // *
    COMMA,        // ,
    LPAREN,       // (
    RPAREN,       // )
    SEMICOLON,    // ;
    DOT,          // .
    ASSIGN,       // =

    // Специальные
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
    size_t position; // Позиция в исходной строке (для ошибок)

    Token() : type(TokenType::UNKNOWN), position(0) {}
    Token(TokenType t, const std::string& v, size_t pos)
        : type(t), value(v), position(pos) {}
};

// Для удобства отладки
inline std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::KW_CREATE: return "CREATE";
        case TokenType::KW_DROP: return "DROP";
        case TokenType::KW_DATABASE: return "DATABASE";
        case TokenType::KW_TABLE: return "TABLE";
        case TokenType::KW_USE: return "USE";
        case TokenType::KW_INSERT: return "INSERT";
        case TokenType::KW_INTO: return "INTO";
        case TokenType::KW_VALUE: return "VALUE";
        case TokenType::KW_UPDATE: return "UPDATE";
        case TokenType::KW_SET: return "SET";
        case TokenType::KW_DELETE: return "DELETE";
        case TokenType::KW_FROM: return "FROM";
        case TokenType::KW_SELECT: return "SELECT";
        case TokenType::KW_WHERE: return "WHERE";
        case TokenType::KW_AS: return "AS";
        case TokenType::KW_AND: return "AND";
        case TokenType::KW_OR: return "OR";
        case TokenType::KW_NOT: return "NOT";
        case TokenType::KW_NULL: return "NULL";
        case TokenType::KW_BETWEEN: return "BETWEEN";
        case TokenType::KW_LIKE: return "LIKE";
        case TokenType::KW_INT: return "INT";
        case TokenType::KW_STRING: return "STRING";
        case TokenType::KW_NOT_NULL: return "NOT_NULL";
        case TokenType::KW_INDEXED: return "INDEXED";
        case TokenType::KW_DEFAULT: return "DEFAULT";
        case TokenType::KW_REVERT: return "REVERT";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::TIMESTAMP_LITERAL: return "TIMESTAMP_LITERAL";
        case TokenType::OP_EQ: return "==";
        case TokenType::OP_NEQ: return "!=";
        case TokenType::OP_LT: return "<";
        case TokenType::OP_GT: return ">";
        case TokenType::OP_LTE: return "<=";
        case TokenType::ASTERISK: return "*";
        case TokenType::OP_GTE: return ">=";
        case TokenType::COMMA: return ",";
        case TokenType::LPAREN: return "(";
        case TokenType::RPAREN: return ")";
        case TokenType::SEMICOLON: return ";";
        case TokenType::DOT: return ".";
        case TokenType::ASSIGN: return "=";
        case TokenType::END_OF_FILE: return "EOF";
        default: return "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "[" << tokenTypeToString(token.type) << ": '" << token.value << "' at " << token.position << "]";
    return os;
}

} // namespace sysdb

#endif // TOKEN_HPP