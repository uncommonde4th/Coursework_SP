#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include "core/types/include/value.hpp"

namespace sysdb {

    enum class CommandType {
<<<<<<< HEAD
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
=======
        CREATE_DATABASE, DROP_DATABASE, USE_DATABASE,
        CREATE_TABLE, DROP_TABLE, INSERT,
        DELETE_CMD, UPDATE_CMD, SELECT_CMD, UNKNOWN
>>>>>>> additional_tasks_2
    };

    struct Command {
        virtual ~Command() = default;
        virtual CommandType getType() const = 0;
    };

    struct CreateDatabaseCmd : public Command {
        std::string name;
        CommandType getType() const override { return CommandType::CREATE_DATABASE; }
    };

    struct DropDatabaseCmd : public Command {
        std::string name;
        CommandType getType() const override { return CommandType::DROP_DATABASE; }
    };

    struct UseDatabaseCmd : public Command {
        std::string name;
        CommandType getType() const override { return CommandType::USE_DATABASE; }
    };

    struct ColumnDef {
        std::string name;
        std::string type;
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
        std::vector<std::string> column_names;
        std::vector<std::vector<Value>> rows;
        CommandType getType() const override { return CommandType::INSERT; }
    };

    struct DropTableCmd : public Command {
        std::string table_name;
        CommandType getType() const override { return CommandType::DROP_TABLE; }
    };

    // ============================================================
    // Задание 11: Дерево условий WHERE
    // ============================================================
    enum class CondOp { EQ, NEQ, LT, GT, LTE, GTE, BETWEEN, LIKE };
    enum class LogicOp { AND, OR };

    struct ConditionNode;
    using ConditionPtr = std::shared_ptr<ConditionNode>;

    struct ConditionNode {
        enum Type { COMPARISON, LOGICAL } type;
        Operand left;
        CondOp op;
        Operand right;
        Operand third;
        LogicOp logic_op;
        ConditionPtr lhs;
        ConditionPtr rhs;

        static ConditionPtr makeComparison(const Operand& l, CondOp o, const Operand& r, const Operand& t = {}) {
            auto node = std::make_shared<ConditionNode>();
            node->type = COMPARISON;
            node->left = l; node->op = o; node->right = r; node->third = t;
            return node;
        }

        static ConditionPtr makeLogical(LogicOp o, ConditionPtr l, ConditionPtr r) {
            auto node = std::make_shared<ConditionNode>();
            node->type = LOGICAL;
            node->logic_op = o;
            node->lhs = std::move(l);
            node->rhs = std::move(r);
            return node;
        }
    };

    struct Condition {
        ConditionPtr root;
        bool isEmpty() const { return root == nullptr; }
    };
    // ============================================================

    struct DeleteCmd : public Command {
        std::string table_name;
        Condition where;
        bool has_where = false;
        CommandType getType() const override { return CommandType::DELETE_CMD; }
    };

    struct UpdateCmd : public Command {
        std::string table_name;
        std::vector<std::pair<std::string, Value>> set_clause;
        Condition where;
        bool has_where = false;
        CommandType getType() const override { return CommandType::UPDATE_CMD; }
    };

    // ============================================================
    // Задание 12: Агрегатные функции
    // ============================================================
    enum class AggFunc { NONE, SUM, COUNT, AVG };

    struct SelectColumn {
        std::string name;       // Имя колонки (для обычных) или аргумент агрегата
        std::string alias;      // Алиас (если задан через AS)
        bool is_star = false;   // Флаг для SELECT *
        AggFunc agg = AggFunc::NONE; // Тип агрегатной функции
    };
    // ============================================================

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