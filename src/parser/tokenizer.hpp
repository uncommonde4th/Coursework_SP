#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

#include "parser/token.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>

namespace sysdb {

    class Tokenizer {
    public:
        explicit Tokenizer(const std::string& input);

        // Получить все токены из входной строки
        std::vector<Token> tokenize();

        // Получить сообщение об ошибке, если токенизация не удалась
        const std::string& getError() const { return error_; }
        bool hasError() const { return !error_.empty(); }

    private:
        std::string input_;
        size_t current_pos_;
        std::string error_;

        // Карта ключевых слов для быстрого поиска
        static const std::unordered_map<std::string, TokenType> keywords_;

        // Вспомогательные методы
        char peek() const;
        char advance();
        void skipWhitespace();
        Token scanIdentifierOrKeyword();
        Token scanNumber();
        Token scanString();
        Token scanOperator();
        // Пытается распознать литерал временной метки вида
        // yyyy.mm.dd-hh:mm:ss.msmsms начиная с текущей позиции.
        // Возвращает true, если распознавание успешно (токен добавлен в out).
        bool tryScanTimestamp(Token& out);
    };

} // namespace sysdb

#endif // TOKENIZER_HPP