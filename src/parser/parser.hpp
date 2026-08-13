#ifndef PARSER_HPP
#define PARSER_HPP

#include "parser/token.hpp"
#include "parser/commands.hpp"
#include "storage_engine.hpp"
#include <vector>
#include <string>

namespace sysdb {

    class Parser {
    public:
        explicit Parser(StorageStub& storage);

        CommandPtr parse(const std::vector<Token>& tokens);

        const std::string& getError() const { return error_; }
        bool hasError() const { return !error_.empty(); }

    private:
        StorageStub& storage_;
        std::vector<Token> tokens_;
        size_t current_pos_;
        std::string error_;

        Token peek();
        Token consume();
        bool match(TokenType type);
        void expect(TokenType type);
        bool checkKeyword(const std::string& keyword);

        CommandPtr parseCreateDatabase();
        CommandPtr parseDropDatabase();
        CommandPtr parseUseDatabase();
        CommandPtr parseCreateTable();
        CommandPtr parseInsert();
        CommandPtr parseDropTable();
        CommandPtr parseDelete();
        CommandPtr parseUpdate();
        CommandPtr parseSelect();
        bool parseTableReference(std::string& database, std::string& table);
        bool parseOperand(Operand& operand);

        // === НОВЫЕ МЕТОДЫ ДЛЯ ЗАДАНИЯ 11 ===
        ConditionPtr parseOrExpr();
        ConditionPtr parseAndExpr();
        ConditionPtr parseAtom();
        // ====================================
    };

} // namespace sysdb

#endif // PARSER_HPP