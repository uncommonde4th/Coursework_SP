#include "parser/tokenizer.hpp"
#include <algorithm>
#include <stdexcept>

namespace sysdb {

// Инициализация карты ключевых слов
const std::unordered_map<std::string, TokenType> Tokenizer::keywords_ = {
    {"CREATE", TokenType::KW_CREATE},
    {"DROP", TokenType::KW_DROP},
    {"DATABASE", TokenType::KW_DATABASE},
    {"TABLE", TokenType::KW_TABLE},
    {"USE", TokenType::KW_USE},
    {"INSERT", TokenType::KW_INSERT},
    {"INTO", TokenType::KW_INTO},
    {"VALUE", TokenType::KW_VALUE},
    {"UPDATE", TokenType::KW_UPDATE},
    {"SET", TokenType::KW_SET},
    {"DELETE", TokenType::KW_DELETE},
    {"FROM", TokenType::KW_FROM},
    {"SELECT", TokenType::KW_SELECT},
    {"WHERE", TokenType::KW_WHERE},
    {"AS", TokenType::KW_AS},
    {"AND", TokenType::KW_AND},
    {"OR", TokenType::KW_OR},
    {"NOT", TokenType::KW_NOT},
    {"NULL", TokenType::KW_NULL},
    {"BETWEEN", TokenType::KW_BETWEEN},
    {"LIKE", TokenType::KW_LIKE},
    {"INT", TokenType::KW_INT},
    {"STRING", TokenType::KW_STRING},
    {"NOT_NULL", TokenType::KW_NOT_NULL},
    {"INDEXED", TokenType::KW_INDEXED},
    {"DEFAULT", TokenType::KW_DEFAULT},
    {"REVERT", TokenType::KW_REVERT}
};

Tokenizer::Tokenizer(const std::string& input)
    : input_(input), current_pos_(0) {}

char Tokenizer::peek() const {
    if (current_pos_ >= input_.length()) {
        return '\0';
    }
    return input_[current_pos_];
}

char Tokenizer::advance() {
    if (current_pos_ >= input_.length()) {
        return '\0';
    }
    return input_[current_pos_++];
}

void Tokenizer::skipWhitespace() {
    while (current_pos_ < input_.length() && std::isspace(input_[current_pos_])) {
        current_pos_++;
    }
}

Token Tokenizer::scanIdentifierOrKeyword() {
    size_t start = current_pos_;
    std::string word;

    // Первый символ уже проверен как буква или _
    word += advance();

    while (current_pos_ < input_.length() &&
           (std::isalnum(input_[current_pos_]) || input_[current_pos_] == '_')) {
        word += advance();
    }

    // Проверка на смешение регистра запрещено заданием, но для простоты
    // мы будем приводить к верхнему регистру для поиска в карте,
    // а оригинальное значение сохраним для IDENTIFIER.

    std::string upper_word = word;
    std::transform(upper_word.begin(), upper_word.end(), upper_word.begin(), ::toupper);

    auto it = keywords_.find(upper_word);
    if (it != keywords_.end()) {
        // Это ключевое слово. Проверяем, что оно написано в одном регистре.
        // Задание: "смешение регистров в одном слове не допускается".
        // Если слово найдено в карте, значит оно совпало с эталоном (верхний регистр).
        // Но нам нужно убедиться, что исходное слово либо полностью верхнее, либо полностью нижнее?
        // Нет, задание говорит "Ключевые слова регистронезависимы".
        // Обычно это значит, что CREATE, create, Create - ок.
        // А "смешение" значит CrEaTe - ошибка.

        bool hasUpper = false;
        bool hasLower = false;

        for (char c : word) {
            if (std::isupper(c)) hasUpper = true;
            if (std::islower(c)) hasLower = true;
        }

        if (hasUpper && hasLower) {
            error_ = "Error: Mixed case in keyword '" + word + "' at position " + std::to_string(start);
            return Token(TokenType::UNKNOWN, word, start);
        }

        return Token(it->second, word, start);
    }

    // Проверка имени: не может начинаться с цифры (уже гарантировано вызовом этого метода)
    // Может содержать латиницу, цифры, _
    return Token(TokenType::IDENTIFIER, word, start);
}

// Формат: yyyy.mm.dd-hh:mm:ss.msmsms (ровно 23 символа), используется
// как аргумент команды REVERT. Метод не потребляет вход, если формат
// не подходит - тогда вызывающая сторона откатится к обычному scanNumber().
bool Tokenizer::tryScanTimestamp(Token& out) {
    const size_t start = current_pos_;
    const size_t len = 23; // "yyyy.mm.dd-hh:mm:ss.mmm"
    if (start + len > input_.length()) return false;

    auto isDigits = [&](size_t off, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(input_[start + off + i]))) return false;
        }
        return true;
    };

    if (!isDigits(0, 4)) return false;
    if (input_[start + 4] != '.') return false;
    if (!isDigits(5, 2)) return false;
    if (input_[start + 7] != '.') return false;
    if (!isDigits(8, 2)) return false;
    if (input_[start + 10] != '-') return false;
    if (!isDigits(11, 2)) return false;
    if (input_[start + 13] != ':') return false;
    if (!isDigits(14, 2)) return false;
    if (input_[start + 16] != ':') return false;
    if (!isDigits(17, 2)) return false;
    if (input_[start + 19] != '.') return false;
    if (!isDigits(20, 3)) return false;

    // Убеждаемся, что сразу после метки не идёт ещё одна цифра/точка/двоеточие -
    // иначе это не валидный литерал целиком (защита от частичного совпадения).
    if (start + len < input_.length()) {
        char next = input_[start + len];
        if (std::isdigit(static_cast<unsigned char>(next)) || next == '.' || next == ':') return false;
    }

    std::string value = input_.substr(start, len);
    out = Token(TokenType::TIMESTAMP_LITERAL, value, start);
    current_pos_ += len;
    return true;
}

Token Tokenizer::scanNumber() {
    size_t start = current_pos_;
    std::string num;

    if (peek() == '-') num += advance();
    while (current_pos_ < input_.length() && std::isdigit(input_[current_pos_])) {
        num += advance();
    }

    return Token(TokenType::NUMBER, num, start);
}

Token Tokenizer::scanString() {
    size_t start = current_pos_;
    advance(); // Пропускаем открывающую кавычку "
    std::string str;

    while (current_pos_ < input_.length() && input_[current_pos_] != '"') {
        str += advance();
    }

    if (current_pos_ >= input_.length()) {
        error_ = "Error: Unterminated string literal starting at position " + std::to_string(start);
        return Token(TokenType::UNKNOWN, str, start);
    }

    advance(); // Пропускаем закрывающую кавычку "
    return Token(TokenType::STRING_LITERAL, str, start);
}

Token Tokenizer::scanOperator() {
    size_t start = current_pos_;
    char c = advance();

    switch (c) {
        case '=':
            if (peek() == '=') {
                advance();
                return Token(TokenType::OP_EQ, "==", start);
            }
            return Token(TokenType::ASSIGN, "=", start);
        case '!':
            if (peek() == '=') {
                advance();
                return Token(TokenType::OP_NEQ, "!=", start);
            }
            break;
        case '<':
            if (peek() == '=') {
                advance();
                return Token(TokenType::OP_LTE, "<=", start);
            }
            return Token(TokenType::OP_LT, "<", start);
        case '>':
            if (peek() == '=') {
                advance();
                return Token(TokenType::OP_GTE, ">=", start);
            }
            return Token(TokenType::OP_GT, ">", start);
    }

    error_ = "Error: Unexpected character '" + std::string(1, c) + "' at position " + std::to_string(start);
    return Token(TokenType::UNKNOWN, std::string(1, c), start);
}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    error_.clear();

    while (current_pos_ < input_.length()) {
        skipWhitespace();

        if (current_pos_ >= input_.length()) {
            break;
        }

        char c = peek();
        size_t pos = current_pos_;

        // Комментарии --
        if (c == '-' && current_pos_ + 1 < input_.length() && input_[current_pos_ + 1] == '-') {
            // Пропускаем до конца строки
            while (current_pos_ < input_.length() && input_[current_pos_] != '\n') {
                current_pos_++;
            }
            continue;
        }

        if (std::isalpha(c) || c == '_') {
            tokens.push_back(scanIdentifierOrKeyword());
        } else if (std::isdigit(c)) {
            Token ts;
            if (tryScanTimestamp(ts)) {
                tokens.push_back(ts);
            } else {
                tokens.push_back(scanNumber());
            }
        } else if (c == '-' && current_pos_ + 1 < input_.length() && std::isdigit(input_[current_pos_ + 1])) {
            tokens.push_back(scanNumber());
        } else if (c == '"') {
            tokens.push_back(scanString());
        } else if (c == ',' || c == '(' || c == ')' || c == ';' || c == '.' || c == '*') {
            advance();
            TokenType type = TokenType::UNKNOWN;
            if (c == ',') type = TokenType::COMMA;
            else if (c == '(') type = TokenType::LPAREN;
            else if (c == ')') type = TokenType::RPAREN;
            else if (c == ';') type = TokenType::SEMICOLON;
            else if (c == '*') type = TokenType::ASTERISK;
            else if (c == '.') type = TokenType::DOT;
            tokens.push_back(Token(type, std::string(1, c), pos));
        } else if (c == '=' || c == '!' || c == '<' || c == '>') {
            tokens.push_back(scanOperator());
        } else {
            error_ = "Error: Unexpected character '" + std::string(1, c) + "' at position " + std::to_string(pos);
            tokens.push_back(Token(TokenType::UNKNOWN, std::string(1, c), pos));
            // Можно либо остановиться, либо продолжить. Для надежности лучше остановить.
            break;
        }

        if (!error_.empty()) {
            break;
        }
    }

    if (error_.empty()) {
        tokens.push_back(Token(TokenType::END_OF_FILE, "", current_pos_));
    }

    return tokens;
}

} // namespace sysdb