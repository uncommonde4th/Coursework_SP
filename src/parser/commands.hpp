#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <string>
#include <variant>
#include <memory>

namespace sysdb {

    // Типы команд
    enum class CommandType {
        CREATE_DATABASE,
        DROP_DATABASE,
        USE_DATABASE,
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

    // Умный указатель на команду
    using CommandPtr = std::unique_ptr<Command>;

} // namespace sysdb

#endif // COMMANDS_HPP