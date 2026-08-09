#ifndef PARSER_HPP
#define PARSER_HPP

#include "parser/token.hpp"
#include "parser/commands.hpp"
#include "storage_stub.hpp"
#include <vector>
#include <string>

namespace sysdb {

    class Parser {
    public:
        explicit Parser(StorageStub& storage);

        // Главный метод: принимает вектор токенов и возвращает команду
        CommandPtr parse(const std::vector<Token>& tokens);

        // Получить сообщение об ошибке
        const std::string& getError() const { return error_; }
        bool hasError() const { return !error_.empty(); }

    private:
        StorageStub& storage_;
        std::vector<Token> tokens_;
        size_t current_pos_;
        std::string error_;

        // Вспомогательные методы
        Token peek();
        Token consume();
        bool match(TokenType type);
        void expect(TokenType type);
        bool checkKeyword(const std::string& keyword);

        // Методы парсинга конкретных команд
        CommandPtr parseCreateDatabase();
        CommandPtr parseDropDatabase();
        CommandPtr parseUseDatabase();
        CommandPtr parseCreateTable();
        CommandPtr parseInsert();
        CommandPtr parseDropTable();
        CommandPtr parseDelete();
        CommandPtr parseUpdate();
        CommandPtr parseSelect();
    };

} // namespace sysdb

#endif // PARSER_HPP