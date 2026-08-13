#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include "core/types/include/value.hpp"

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
        REVERT_CMD,
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
        bool has_default = false; // задание 10: DEFAULT [value]
        Value default_value = Value::make_null();
    };

    struct CreateTableCmd : public Command {
        std::string table_name;
        std::vector<ColumnDef> columns;

        CommandType getType() const override { return CommandType::CREATE_TABLE; }
    };

    struct Operand {
        bool is_column = false;
        std::string column;
        Value value = Value::make_null();
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
        Operand left;
        Operand right;
        Operand third;
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

    // REVERT [table_name] [yyyy.mm.dd-hh:mm:ss.msmsms]; (доп. задание 1)
    struct RevertCmd : public Command {
        std::string table_name;
        std::string timestamp; // хранится как есть, разбор - в StorageStub
        CommandType getType() const override { return CommandType::REVERT_CMD; }
    };

    using CommandPtr = std::unique_ptr<Command>;

} // namespace sysdb

#endif // COMMANDS_HPP