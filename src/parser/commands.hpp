#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <string>
#include <vector>
#include <variant>
#include <memory>

namespace sysdb {

    // Типы команд
    enum class CommandType {
        CREATE_DATABASE,
        DROP_DATABASE,
        USE_DATABASE,
        CREATE_TABLE,
        DROP_TABLE,
        INSERT,
        DELETE_CMD,
        UPDATE_CMD,
        SELECT_CMD,
        UNKNOWN
    };

    // Базовый класс для всех команд
    struct Command {
        virtual ~Command() = default;
        virtual CommandType getType() const = 0;
    };

    // CREATE DATABASE [name]
    struct CreateDatabaseCmd : public Command {
        std::string name;
        CommandType getType() const override { return CommandType::CREATE_DATABASE; }
    };

    // DROP DATABASE [name]
    struct DropDatabaseCmd : public Command {
        std::string name;
        CommandType getType() const override { return CommandType::DROP_DATABASE; }
    };

    // USE [name]
    struct UseDatabaseCmd : public Command {
        std::string name;
        CommandType getType() const override { return CommandType::USE_DATABASE; }
    };

    struct ColumnDef {
        std::string name;
        std::string type; // "INT" или "STRING"
        bool not_null = false;
        bool indexed = false;
    };

    struct CreateTableCmd : public Command {
        std::string table_name;
        std::vector<ColumnDef> columns;

        CommandType getType() const override { return CommandType::CREATE_TABLE; }
    };

    struct Value {
        enum Type { INT_VAL, STRING_VAL, NULL_VAL } type;
        int64_t int_value;
        std::string string_value;

        static Value make_int(int64_t v) { return {INT_VAL, v, ""}; }
        static Value make_string(const std::string& v) { return {STRING_VAL, 0, v}; }
        static Value make_null() { return {NULL_VAL, 0, ""}; }
    };

    struct InsertCmd : public Command {
        std::string table_name;
        std::vector<std::string> column_names; // Может быть пустым, если указаны все колонки
        std::vector<std::vector<Value>> rows;  // Несколько кортежей значений

        CommandType getType() const override { return CommandType::INSERT; }
    };

    struct DropTableCmd : public Command {
        std::string table_name;
        CommandType getType() const override { return CommandType::DROP_TABLE; }
    };

    // Простая структура для условия WHERE (пока только одно сравнение)
    struct Condition {
        std::string column;
        std::string op; // "==", "!=", "<", ">", "<=", ">="
        Value value;
    };

    struct DeleteCmd : public Command {
        std::string table_name;
        Condition where; // Условие удаления
        bool has_where = false; // Флаг наличия WHERE

        CommandType getType() const override { return CommandType::DELETE_CMD; }
    };

    struct UpdateCmd : public Command {
        std::string table_name;
        std::vector<std::pair<std::string, Value>> set_clause;
        Condition where;
        bool has_where = false;

        CommandType getType() const override { return CommandType::UPDATE_CMD; }
    };

    struct SelectColumn {
        std::string name;
        std::string alias; // Может быть пустым
        bool is_star = false; // Флаг для SELECT *
    };

    struct SelectCmd : public Command {
        std::string table_name;
        std::vector<SelectColumn> columns;
        Condition where;
        bool has_where = false;

        CommandType getType() const override { return CommandType::SELECT_CMD; }
    };

    using CommandPtr = std::unique_ptr<Command>;

} // namespace sysdb

#endif // COMMANDS_HPP