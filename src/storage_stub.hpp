#ifndef STORAGE_STUB_HPP
#define STORAGE_STUB_HPP

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "parser/commands.hpp" // Для ColumnDef

namespace sysdb {

class StorageStub {
public:
    bool databaseExists(const std::string& name) {
        return databases_.find(name) != databases_.end();
    }

    void createDatabase(const std::string& name) {
        if (!databaseExists(name)) {
            databases_[name] = {};
            std::cout << "[STORAGE] Database '" << name << "' created." << std::endl;
        }
    }

    void dropDatabase(const std::string& name) {
        if (databaseExists(name)) {
            databases_.erase(name);
            std::cout << "[STORAGE] Database '" << name << "' dropped." << std::endl;
        }
    }

    void useDatabase(const std::string& name) {
        if (databaseExists(name)) {
            current_db_ = name;
            std::cout << "[STORAGE] Switched to database '" << name << "'." << std::endl;
        }
    }

    // Новые методы для работы с таблицами
    bool tableExists(const std::string& db, const std::string& table) {
        auto it = databases_.find(db);
        if (it == databases_.end()) return false;
        return it->second.find(table) != it->second.end();
    }

    void createTable(const std::string& db, const std::string& table, const std::vector<ColumnDef>& cols) {
        if (databases_.find(db) != databases_.end()) {
            databases_[db][table] = cols;
            std::cout << "[STORAGE] Table '" << table << "' created in '" << db << "'." << std::endl;
        }
    }

    const std::vector<ColumnDef>* getTableSchema(const std::string& db, const std::string& table) {
        auto db_it = databases_.find(db);
        if (db_it == databases_.end()) return nullptr;

        auto tbl_it = db_it->second.find(table);
        if (tbl_it == db_it->second.end()) return nullptr;

        return &tbl_it->second;
    }

    const std::string& getCurrentDatabase() const { return current_db_; }

    bool insertRows(const std::string& db, const std::string& table, const std::vector<std::vector<Value>>& rows) {
        auto schema = getTableSchema(db, table);
        if (!schema) return false;

        for (const auto& row : rows) {
            // Простая проверка количества
            if (row.size() != schema->size()) {
                std::cerr << "[STORAGE ERROR] Row size mismatch for table '" << table << "'" << std::endl;
                return false;
            }

            // Проверка NOT_NULL
            for (size_t i = 0; i < row.size(); ++i) {
                if (row[i].type == Value::NULL_VAL && (*schema)[i].not_null) {
                    std::cerr << "[STORAGE ERROR] Column '" << (*schema)[i].name << "' cannot be NULL" << std::endl;
                    return false;
                }
            }
        }

        std::cout << "[STORAGE] Inserted " << rows.size() << " row(s) into '" << table << "'." << std::endl;
        return true;
    }

private:
    std::string current_db_;
    // Map<DatabaseName, Map<TableName, Columns>>
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<ColumnDef>>> databases_;
};

} // namespace sysdb

#endif // STORAGE_STUB_HPP