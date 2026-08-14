#ifndef STORAGE_STUB_HPP
#define STORAGE_STUB_HPP

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include "parser/commands.hpp" // Для ColumnDef
#include "record_manager.hpp"
#include "bplus_tree.hpp"

namespace sysdb {

class StorageStub {
public:
    StorageStub() : root_("sysdb_data") {
        std::filesystem::create_directories(root_);
        loadMetadata();
    }

    bool databaseExists(const std::string& name) const {
        return databases_.find(name) != databases_.end();
    }

    void createDatabase(const std::string& name) {
        clearError();
        if (databaseExists(name)) {
            setError("Database '" + name + "' already exists");
            return;
        }
        std::filesystem::create_directories(root_ / name);
        databases_.try_emplace(name);
        flushMetadata();
        std::cout << "[STORAGE] Database '" << name << "' created." << std::endl;
    }

    void dropDatabase(const std::string& name) {
        clearError();
        if (!databaseExists(name)) {
            setError("Database '" + name + "' does not exist");
            return;
        }
        if (current_db_ == name) current_db_.clear();

        databases_.erase(name);

        std::error_code ec;
        std::filesystem::remove_all(root_ / name, ec);

        flushMetadata();
        std::cout << "[STORAGE] Database '" << name << "' dropped." << std::endl;
    }

    void useDatabase(const std::string& name) {
        clearError();
        if (!databaseExists(name)) {
            setError("Database '" + name + "' does not exist");
            return;
        }
        current_db_ = name;
        std::cout << "[STORAGE] Switched to database '" << name << "'." << std::endl;
    }

    bool tableExists(const std::string& db, const std::string& table) const {
        auto it = databases_.find(db);
        if (it == databases_.end()) return false;
        return it->second.find(table) != it->second.end();
    }

    void createTable(const std::string& db, const std::string& table, const std::vector<ColumnDef>& cols) {
        clearError();
        if (!databaseExists(db)) {
            setError("Database '" + db + "' does not exist");
            return;
        }
        if (tableExists(db, table)) {
            setError("Table '" + table + "' already exists in database '" + db + "'");
            return;
        }
        if (cols.empty()) {
            setError("Table must contain at least one column");
            return;
        }
        std::unordered_map<std::string, bool> column_names;
        for (const auto& col : cols) {
            if (column_names[col.name]) {
                setError("Column '" + col.name + "' specified more than once");
                return;
            }
            column_names[col.name] = true;
        }

        // Новые методы для работы с таблицами
        Table table_data;
        table_data.columns = cols;
        for (const auto& col : cols) {
            if (col.indexed && col.type != "INT" && col.type != "int" && col.type != "STRING" && col.type != "string") {
                setError("Invalid indexed column type for '" + col.name + "'");
                return;
            }
            // Задание 10: значения по умолчанию (DEFAULT).
            if (col.has_default && !col.default_value.is_null() && col.default_value.type() != columnType(col)) {
                setError("Default value type mismatch for column '" + col.name + "'");
                return;
            }
            if (col.has_default && col.default_value.is_null() && col.not_null) {
                setError("Column '" + col.name + "' cannot have NULL default value because it is NOT_NULL");
                return;
            }
            if (col.indexed) {
                table_data.indexes[col.name] = nullptr;
            }
        }

        const auto path = tablePath(db, table);
        table_data.records = std::make_unique<RecordManager>(path.string());
        if (!table_data.records->initialize()) {
            setError("Cannot create data file for table '" + table + "'");
            return;
        }

        for (const auto& col : cols) {
            if (!col.indexed) continue;
            auto type = columnType(col);
            const auto index_path = (path.string() + "." + col.name);
            auto index = std::make_unique<BPlusTree>(index_path, type);
            if (!index->initialize()) {
                setError("Cannot create index for column '" + col.name + "'");
                return;
            }
            table_data.indexes[col.name] = std::move(index);
        }

        databases_[db][table] = std::move(table_data);
        saveSchema(db, table);
        flushTable(db, table);
        std::cout << "[STORAGE] Table '" << table << "' created in '" << db << "'." << std::endl;
    }

    const std::vector<ColumnDef>* getTableSchema(const std::string& db, const std::string& table) const {
        auto db_it = databases_.find(db);
        if (db_it == databases_.end()) return nullptr;
        auto tbl_it = db_it->second.find(table);
        if (tbl_it == db_it->second.end()) return nullptr;
        return &tbl_it->second.columns;
    }

    const std::string& getCurrentDatabase() const { return current_db_; }
    const std::string& getError() const { return error_; }
    bool hasError() const { return !error_.empty(); }

    bool insertRows(const std::string& db, const std::string& table,
                    const std::vector<std::vector<Value>>& rows,
                    const std::vector<std::string>& column_names = {}) {
        clearError();
        Table* t = findTable(db, table);
        if (!t) return false;

        std::vector<size_t> mapping;
        if (column_names.empty()) {
            mapping.resize(t->columns.size());
            for (size_t i = 0; i < mapping.size(); ++i) mapping[i] = i;
        } else {
            std::unordered_map<std::string, bool> seen;
            for (const auto& name : column_names) {
                int idx = columnIndex(*t, name);
                if (idx < 0) {
                    setError("Column '" + name + "' does not exist in table '" + table + "'");
                    return false;
                }
                if (seen[name]) {
                    setError("Column '" + name + "' specified more than once");
                    return false;
                }
                seen[name] = true;
                mapping.push_back(static_cast<size_t>(idx));
            }
        }

        // Задание 10: столбцы, не переданные явно в INSERT, получают значение
        // DEFAULT (если оно задано в CREATE TABLE), а не NULL.
        std::vector<bool> provided(t->columns.size(), false);
        for (size_t idx : mapping) provided[idx] = true;

        std::vector<RecordId> inserted;
        for (const auto& input : rows) {
            // Простая проверка количества
            if (input.size() != mapping.size()) {
                setError("Value count does not match column count in INSERT");
                rollbackInserted(*t, inserted);
                return false;
            }

            std::vector<Value> values(t->columns.size(), Value::make_null());
            for (size_t i = 0; i < input.size(); ++i) values[mapping[i]] = input[i];
            for (size_t i = 0; i < t->columns.size(); ++i) {
                if (!provided[i] && t->columns[i].has_default) values[i] = t->columns[i].default_value;
            }
            if (!validateRow(*t, values)) {
                rollbackInserted(*t, inserted);
                return false;
            }

            for (const auto& col : t->columns) {
                if (!col.indexed) continue;
                int idx = columnIndex(*t, col.name);
                auto& index = t->indexes[col.name];
                if (index->find(values[idx]).size() != 0) {
                    setError("Duplicate value for indexed column '" + col.name + "'");
                    rollbackInserted(*t, inserted);
                    return false;
                }
            }

            RecordId rid = t->records->insert_record(values, makeSchema(*t));
            if (!rid.is_valid()) {
                setError("Failed to insert record into table '" + table + "'");
                rollbackInserted(*t, inserted);
                return false;
            }
            inserted.push_back(rid);
            for (const auto& col : t->columns) {
                if (col.indexed) {
                    int idx = columnIndex(*t, col.name);
                    t->indexes[col.name]->insert(values[idx], rid);
                }
            }
            appendWal(db, table, 'I', rid, values);
        }

        flushTable(db, table);
        std::cout << "[STORAGE] Inserted " << rows.size() << " row(s) into '" << table << "'." << std::endl;
        return true;
    }

    void dropTable(const std::string& db, const std::string& table) {
        clearError();
        auto db_it = databases_.find(db);
        if (db_it == databases_.end()) {
            setError("Database '" + db + "' does not exist");
            return;
        }
        auto it = db_it->second.find(table);
        if (it == db_it->second.end()) {
            setError("Table '" + table + "' does not exist");
            return;
        }

        std::vector<ColumnDef> columns = it->second.columns;
        const std::string base_path = tablePath(db, table).string();

        db_it->second.erase(it);

        std::error_code ec;
        std::filesystem::remove(base_path + ".dat", ec);
        std::filesystem::remove(base_path + ".schema", ec);
        std::filesystem::remove(base_path + ".wal", ec);
        for (const auto& col : columns) {
            if (col.indexed) {
                std::filesystem::remove(base_path + "." + col.name + ".idx", ec);
            }
        }

        std::cout << "[STORAGE] Table '" << table << "' dropped from '" << db << "'." << std::endl;
    }

    bool deleteRows(const std::string& db /*db*/, const std::string& table, const Condition& cond, bool has_where) {
        clearError();
        Table* t = findTable(db, table);
        if (!t) return false;
        auto rids = matchingRecords(*t, cond, has_where);
        for (const auto& rid : rids) {
            auto row = t->records->get_record(rid, makeSchema(*t));
            if (row.empty()) continue;
            if (!t->records->delete_record(rid)) {
                setError("Failed to delete record");
                return false;
            }
            for (const auto& col : t->columns) {
                if (col.indexed) {
                    int idx = columnIndex(*t, col.name);
                    t->indexes[col.name]->remove(row[idx], rid);
                }
            }
            appendWal(db, table, 'D', rid, {});
        }
        flushTable(db, table);
        return true;
    }

    bool updateRows(const std::string& db /*db*/, const std::string& table,
                    const std::vector<std::pair<std::string, Value>>& sets,
                    const Condition& cond, bool has_where) {
        clearError();
        Table* t = findTable(db, table);
        if (!t) return false;
        auto rids = matchingRecords(*t, cond, has_where);
        for (const auto& rid : rids) {
            auto old_row = t->records->get_record(rid, makeSchema(*t));
            if (old_row.empty()) continue;
            auto new_row = old_row;
            for (const auto& item : sets) {
                int idx = columnIndex(*t, item.first);
                if (idx < 0) {
                    setError("Column '" + item.first + "' does not exist");
                    return false;
                }
                new_row[idx] = item.second;
            }
            if (!validateRow(*t, new_row)) return false;
            for (const auto& col : t->columns) {
                if (!col.indexed) continue;
                int idx = columnIndex(*t, col.name);
                if (new_row[idx] == old_row[idx]) continue;
                if (t->indexes[col.name]->find(new_row[idx]).size() != 0) {
                    setError("Duplicate value for indexed column '" + col.name + "'");
                    return false;
                }
            }
            if (!t->records->update_record(rid, new_row, makeSchema(*t))) {
                setError("Failed to update record");
                return false;
            }
            // Сначала гарантированно обновляем запись, затем индекс.
            // RID остаётся тем же, поэтому индекс можно безопасно перестроить.
            for (const auto& col : t->columns) {
                if (!col.indexed) continue;
                int idx = columnIndex(*t, col.name);
                if (!(new_row[idx] == old_row[idx])) {
                    t->indexes[col.name]->remove(old_row[idx], rid);
                    t->indexes[col.name]->insert(new_row[idx], rid);
                }
            }
            appendWal(db, table, 'U', rid, new_row);
        }
        flushTable(db, table);
        return true;
    }

    bool selectRows(const std::string& db, const std::string& table,
                    const std::vector<SelectColumn>& columns,
                    const Condition& cond, bool has_where) {
        clearError();
        const Table* t = findTableConst(db, table);
        if (!t) return false;
        auto rids = matchingRecords(*t, cond, has_where);
        std::vector<SelectColumn> selected;
        if (columns.size() == 1 && columns[0].is_star) {
            for (const auto& col : t->columns) selected.push_back({col.name, "", false});
        } else {
            selected = columns;
            for (const auto& col : selected) {
                if (columnIndex(*t, col.name) < 0) {
                    setError("Column '" + col.name + "' does not exist");
                    return false;
                }
            }
        }

        std::cout << "[";
        bool first_row = true;
        for (const auto& rid : rids) {
            auto row = t->records->get_record(rid, makeSchema(*t));
            if (row.empty()) continue;
            if (!first_row) std::cout << ",";
            first_row = false;
            std::cout << "{";
            for (size_t i = 0; i < selected.size(); ++i) {
                if (i) std::cout << ",";
                const auto& col = selected[i];
                int idx = columnIndex(*t, col.name);
                std::cout << "\"" << jsonEscape(col.alias.empty() ? col.name : col.alias) << "\":";
                printJson(row[idx]);
            }
            std::cout << "}";
        }
        std::cout << "]" << std::endl;
        return true;
    }

    // Доп. задание 1: темпоральная персистентность.
    // REVERT [table] [yyyy.mm.dd-hh:mm:ss.msmsms];
    // Восстанавливает состояние таблицы на заданный момент времени, реплеем
    // журнала операций (WAL): INSERT/UPDATE/DELETE, которые пишутся туда при
    // каждом изменении данных (см. appendWal). Полное копирование файлов БД
    // (snapshot) НЕ используется - меняется только содержимое .dat/.idx на
    // основе результата реплея лога, сам .wal при этом не трогается, так что
    // повторный REVERT к более ранней точке остаётся возможным.
    bool revertTable(const std::string& db, const std::string& table, const std::string& timestamp_str) {
        clearError();
        Table* t = findTable(db, table);
        if (!t) return false;

        bool ok = false;
        int64_t cutoff = parseTimestampToMs(timestamp_str, ok);
        if (!ok) {
            setError("Invalid REVERT timestamp. Expected format yyyy.mm.dd-hh:mm:ss.msmsms");
            return false;
        }

        // 1) Реплеим лог операций до cutoff включительно, восстанавливая
        //    логическое состояние таблицы: rid -> актуальные значения строки.
        std::map<uint64_t, std::vector<Value>> target;
        {
            std::ifstream wal(walPath(db, table));
            std::string line;
            while (wal && std::getline(wal, line)) {
                if (line.empty()) continue;
                std::vector<std::string> parts = splitTabs(line);
                if (parts.size() < 4) continue;

                int64_t ts;
                try { ts = std::stoll(parts[0]); } catch (...) { continue; }
                if (ts > cutoff) continue;

                char op = parts[1].empty() ? '?' : parts[1][0];
                uint64_t key;
                try {
                    uint32_t page_id = static_cast<uint32_t>(std::stoul(parts[2]));
                    uint16_t slot_id = static_cast<uint16_t>(std::stoul(parts[3]));
                    key = (static_cast<uint64_t>(page_id) << 16) | slot_id;
                } catch (...) { continue; }

                if (op == 'D') {
                    target.erase(key);
                    continue;
                }
                if (parts.size() < 5) continue;
                size_t ncols;
                try { ncols = static_cast<size_t>(std::stoul(parts[4])); } catch (...) { continue; }

                std::vector<Value> row;
                row.reserve(ncols);
                for (size_t i = 0; i < ncols && (5 + i) < parts.size(); ++i) {
                    row.push_back(decodeValueForWal(parts[5 + i]));
                }
                target[key] = std::move(row); // актуально и для I, и для U
            }
        }

        t->records.reset();
        for (auto& kv : t->indexes) kv.second.reset();

        const auto path = tablePath(db, table);
        std::error_code ec;
        std::filesystem::remove(path.string() + ".dat", ec);
        for (const auto& col : t->columns) {
            if (col.indexed) std::filesystem::remove(path.string() + "." + col.name + ".idx", ec);
        }

        t->records = std::make_unique<RecordManager>(path.string());
        if (!t->records->initialize()) {
            setError("Failed to rebuild storage for table '" + table + "' during REVERT");
            return false;
        }
        for (const auto& col : t->columns) {
            if (!col.indexed) continue;
            auto type = columnType(col);
            const auto index_path = path.string() + "." + col.name;
            auto index = std::make_unique<BPlusTree>(index_path, type);
            if (!index->initialize()) {
                setError("Failed to rebuild index for column '" + col.name + "' during REVERT");
                return false;
            }
            t->indexes[col.name] = std::move(index);
        }

        // 3) Материализуем восстановленное состояние в порядке возрастания
        //    исходного (старого) RID - на свежесозданном heap-файле это даёт
        //    физический порядок слотов, совпадающий с исходным порядком
        //    вставки. Это не пользовательская операция, а разворачивание уже
        //    записанной истории, поэтому в WAL заново не пишем.
        for (auto& item : target) {
            auto& row = item.second;
            if (row.size() != t->columns.size()) continue; // защита от рассинхронизации схемы
            RecordId new_rid = t->records->insert_record(row, makeSchema(*t));
            if (!new_rid.is_valid()) continue;
            for (const auto& col : t->columns) {
                if (!col.indexed) continue;
                int idx = columnIndex(*t, col.name);
                if (!row[idx].is_null()) t->indexes[col.name]->insert(row[idx], new_rid);
            }
        }

        flushTable(db, table);
        std::cout << "[STORAGE] Table '" << table << "' reverted to " << timestamp_str << "." << std::endl;
        return true;
    }

private:
    struct Table {
        std::vector<ColumnDef> columns;
        std::unique_ptr<RecordManager> records;
        std::unordered_map<std::string, std::unique_ptr<BPlusTree>> indexes;
        Table() = default;
        Table(const Table&) = delete;
        Table& operator=(const Table&) = delete;
        Table(Table&&) noexcept = default;
        Table& operator=(Table&&) noexcept = default;
    };

    // Map<DatabaseName, Map<TableName, Columns>>
    using Database = std::unordered_map<std::string, Table>;

    std::filesystem::path root_;
    std::string current_db_;
    std::string error_;
    std::unordered_map<std::string, Database> databases_;

    void clearError() { error_.clear(); }
    void setError(const std::string& error) { error_ = error; std::cerr << "[STORAGE ERROR] " << error << std::endl; }

    std::filesystem::path tablePath(const std::string& db, const std::string& table) const {
        return root_ / db / table;
    }

    // Доп. задание 1: журнал операций (WAL) для REVERT

    std::filesystem::path walPath(const std::string& db, const std::string& table) const {
        return std::filesystem::path(tablePath(db, table).string() + ".wal");
    }

    static int64_t nowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    static int64_t parseTimestampToMs(const std::string& s, bool& ok) {
        ok = false;
        if (s.size() != 23 || s[4] != '.' || s[7] != '.' || s[10] != '-' ||
            s[13] != ':' || s[16] != ':' || s[19] != '.') {
            return 0;
        }
        try {
            int year = std::stoi(s.substr(0, 4));
            int mon = std::stoi(s.substr(5, 2));
            int day = std::stoi(s.substr(8, 2));
            int hour = std::stoi(s.substr(11, 2));
            int minute = std::stoi(s.substr(14, 2));
            int sec = std::stoi(s.substr(17, 2));
            int ms = std::stoi(s.substr(20, 3));

            std::tm tmv{};
            tmv.tm_year = year - 1900;
            tmv.tm_mon = mon - 1;
            tmv.tm_mday = day;
            tmv.tm_hour = hour;
            tmv.tm_min = minute;
            tmv.tm_sec = sec;
            tmv.tm_isdst = -1;

            time_t t = std::mktime(&tmv);
            if (t == static_cast<time_t>(-1)) return 0;

            ok = true;
            return static_cast<int64_t>(t) * 1000 + ms;
        } catch (...) {
            return 0;
        }

    }

    static std::string walEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '\\') out += "\\\\";
            else if (c == '\t') out += "\\t";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else out += c;
        }
        return out;
    }

    static std::string walUnescape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char n = s[++i];
                if (n == 'n') out += '\n';
                else if (n == 't') out += '\t';
                else if (n == 'r') out += '\r';
                else out += n;
            } else {
                out += s[i];
            }
        }
        return out;
    }

    static std::string encodeValueForWal(const Value& v) {
        if (v.is_null()) return "N";
        if (v.type() == Value::Type::INT) return "I:" + std::to_string(v.as_int());
        return "S:" + walEscape(v.as_string());
    }

    static Value decodeValueForWal(const std::string& s) {
        if (s.empty() || s == "N") return Value::make_null();
        if (s.size() >= 2 && s[1] == ':') {
            char tag = s[0];
            std::string rest = s.substr(2);
            if (tag == 'I') {
                try { return Value::make_int(std::stoll(rest)); }
                catch (...) { return Value::make_null(); }
            }
            if (tag == 'S') return Value::make_string(walUnescape(rest));
        }
        return Value::make_null();
    }

    static std::vector<std::string> splitTabs(const std::string& line) {
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '\t')) parts.push_back(field);
        return parts;
    }

    void appendWal(const std::string& db, const std::string& table, char op,
                   const RecordId& rid, const std::vector<Value>& row) {
        std::ofstream out(walPath(db, table), std::ios::app);
        if (!out) return;
        out << nowMs() << '\t' << op << '\t' << rid.page_id << '\t' << rid.slot_id << '\t' << row.size();
        for (const auto& v : row) out << '\t' << encodeValueForWal(v);
        out << '\n';
    }

    Table* findTable(const std::string& db, const std::string& table) {
        auto db_it = databases_.find(db);
        if (db_it == databases_.end()) { setError("Database '" + db + "' does not exist"); return nullptr; }
        auto it = db_it->second.find(table);
        if (it == db_it->second.end()) { setError("Table '" + table + "' does not exist"); return nullptr; }
        return &it->second;
    }

    const Table* findTableConst(const std::string& db, const std::string& table) const {
        auto db_it = databases_.find(db);
        if (db_it == databases_.end()) return nullptr;
        auto it = db_it->second.find(table);
        if (it == db_it->second.end()) return nullptr;
        return &it->second;
    }

    int columnIndex(const Table& table, const std::string& name) const {
        for (size_t i = 0; i < table.columns.size(); ++i) if (table.columns[i].name == name) return static_cast<int>(i);
        return -1;
    }

    Value::Type columnType(const ColumnDef& col) const {
        return (col.type == "INT" || col.type == "int") ? Value::Type::INT : Value::Type::STRING;
    }

    TableSchema makeSchema(const Table& table) const {
        TableSchema schema;
        for (const auto& col : table.columns) schema.add_column(col.name, columnType(col), col.not_null, col.indexed);
        return schema;
    }

    bool validateRow(const Table& table, const std::vector<Value>& row) {
        if (row.size() != table.columns.size()) { setError("Invalid number of values"); return false; }
        // Проверка NOT_NULL
        for (size_t i = 0; i < row.size(); ++i) {
            const auto& col = table.columns[i];
            if (row[i].is_null()) {
                if (col.not_null || col.indexed) {
                    setError("Column '" + col.name + "' cannot be NULL");
                    return false;
                }
                continue;
            }
            if (row[i].type() != columnType(col)) {
                setError("Type mismatch for column '" + col.name + "'");
                return false;
            }
        }
        return true;
    }

    Value operandValue(const Table& table, const std::vector<Value>& row, const Operand& operand) const {
        if (!operand.is_column) return operand.value;
        int idx = columnIndex(table, operand.column);
        if (idx < 0 || static_cast<size_t>(idx) >= row.size()) return Value::make_null();
        return row[idx];
    }

    bool compare(const Value& left, const Value& right, const std::string& op) const {
        if (left.is_null() || right.is_null() || left.type() != right.type()) return false;
        if (op == "==") return left == right;
        if (op == "!=") return !(left == right);
        if (op == "<") return left < right;
        if (op == ">") return right < left;
        if (op == "<=") return left < right || left == right;
        if (op == ">=") return right < left || left == right;
        return false;
    }

    bool matches(const Table& table, const std::vector<Value>& row, const Condition& cond) const {
        Value left = operandValue(table, row, cond.left);
        if (cond.op == "BETWEEN") {
            Value low = operandValue(table, row, cond.right);
            Value high = operandValue(table, row, cond.third);
            return compare(left, low, ">=") && compare(left, high, "<");
        }
        if (cond.op == "LIKE") {
            Value pattern = operandValue(table, row, cond.right);
            if (left.is_null() || pattern.is_null() || left.type() != Value::Type::STRING || pattern.type() != Value::Type::STRING) return false;
            try { return std::regex_match(left.as_string(), std::regex(pattern.as_string())); }
            catch (...) { return false; }
        }
        return compare(left, operandValue(table, row, cond.right), cond.op);
    }

    std::vector<RecordId> matchingRecords(const Table& table, const Condition& cond, bool has_where) const {
        if (!has_where) return table.records->scan_all_records();
        if (cond.left.is_column && cond.op == "==") {
            auto it = table.indexes.find(cond.left.column);
            if (it != table.indexes.end() && !cond.right.is_column && !cond.right.value.is_null()) return it->second->find(cond.right.value);
        }
        if (cond.left.is_column && !cond.right.is_column && !cond.third.is_column && (cond.op == "BETWEEN" || cond.op == ">=" || cond.op == ">" || cond.op == "<" || cond.op == "<=")) {
            auto it = table.indexes.find(cond.left.column);
            if (it != table.indexes.end() && !cond.right.value.is_null()) {
                Value low = cond.right.value;
                Value high = cond.third.value;
                if (cond.op == "BETWEEN") return it->second->range_find(low, high);
                if (low.type() == Value::Type::INT) {
                    int64_t v = low.as_int();
                    if (cond.op == ">") { low = Value::make_int(v == INT64_MAX ? INT64_MAX : v + 1); high = Value::make_int(INT64_MAX); }
                    else if (cond.op == "<=" ) { high = Value::make_int(v == INT64_MAX ? INT64_MAX : v + 1); low = Value::make_int(INT64_MIN); }
                    else if (cond.op == "<") { high = Value::make_int(v); low = Value::make_int(INT64_MIN); }
                    else high = Value::make_int(INT64_MAX);
                    return it->second->range_find(low, high);
                }
                if (low.type() == Value::Type::STRING) {
                    const std::string max_string(64, static_cast<char>(0xFF));
                    if (cond.op == ">") low = Value::make_string(low.as_string() + std::string(1, '\0'));
                    else if (cond.op == "<=" ) { high = Value::make_string(low.as_string() + std::string(1, '\0')); low = Value::make_string(""); }
                    else if (cond.op == "<") { high = low; low = Value::make_string(""); }
                    else high = Value::make_string(max_string);
                    return it->second->range_find(low, high);
                }
            }
        }
        std::vector<RecordId> result;
        for (const auto& rid : table.records->scan_all_records()) {
            auto row = table.records->get_record(rid, makeSchema(table));
            if (!row.empty() && matches(table, row, cond)) result.push_back(rid);
        }
        return result;
    }

    void rollbackInserted(Table& table, const std::vector<RecordId>& rids) {
        for (const auto& rid : rids) {
            auto row = table.records->get_record(rid, makeSchema(table));
            if (row.empty()) continue;
            for (const auto& col : table.columns) if (col.indexed) {
                int idx = columnIndex(table, col.name);
                table.indexes[col.name]->remove(row[idx], rid);
            }
            table.records->delete_record(rid);
        }
    }

    void flushTable(const std::string& db, const std::string& table) {
        auto* t = findTableConstMutable(db, table);
        if (!t) return;
        t->records->flush();
        for (auto& item : t->indexes) item.second->flush();
    }

    Table* findTableConstMutable(const std::string& db, const std::string& table) {
        auto db_it = databases_.find(db);
        if (db_it == databases_.end()) return nullptr;
        auto it = db_it->second.find(table);
        return it == db_it->second.end() ? nullptr : &it->second;
    }

    static std::string jsonEscape(const std::string& s) {
        std::ostringstream out;
        for (char c : s) {
            if (c == '\\') out << "\\\\";
            else if (c == '"') out << "\\\"";
            else if (c == '\n') out << "\\n";
            else if (c == '\r') out << "\\r";
            else if (c == '\t') out << "\\t";
            else out << c;
        }
        return out.str();
    }

    static void printJson(const Value& value) {
        if (value.is_null()) { std::cout << "null"; return; }
        if (value.type() == Value::Type::INT) { std::cout << value.as_int(); return; }
        std::cout << "\"" << jsonEscape(value.as_string()) << "\"";
    }

    void saveSchema(const std::string& db, const std::string& table) {
        auto* t = findTableConstMutable(db, table);
        if (!t) return;
        std::ofstream out(tablePath(db, table).string() + ".schema", std::ios::trunc);
        for (const auto& col : t->columns) {
            out << col.name << '\t' << col.type << '\t' << col.not_null << '\t' << col.indexed << '\t'
                << col.has_default << '\t' << (col.has_default ? encodeValueForWal(col.default_value) : std::string("N")) << '\n';
        }
    }

    void loadMetadata() {
        for (const auto& db_entry : std::filesystem::directory_iterator(root_)) {
            if (!db_entry.is_directory()) continue;
            const std::string db = db_entry.path().filename().string();
            databases_.try_emplace(db);
            for (const auto& entry : std::filesystem::directory_iterator(db_entry.path())) {
                const std::string filename = entry.path().filename().string();
                if (entry.is_regular_file() && filename.size() > 7 && filename.substr(filename.size() - 7) == ".schema") {
                    const std::string table = filename.substr(0, filename.size() - 7);
                    loadTable(db, table);
                }
            }
        }
    }

    void loadTable(const std::string& db, const std::string& table) {
        std::ifstream in(tablePath(db, table).string() + ".schema");
        if (!in) return;
        Table t;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::vector<std::string> parts = splitTabs(line);
            if (parts.size() < 4) continue;

            ColumnDef col;
            col.name = parts[0];
            col.type = parts[1];
            col.not_null = parts[2] == "1";
            col.indexed = parts[3] == "1";
            if (parts.size() >= 6) {
                col.has_default = parts[4] == "1";
                if (col.has_default) col.default_value = decodeValueForWal(parts[5]);
            }
            t.columns.push_back(col);
        }
        t.records = std::make_unique<RecordManager>(tablePath(db, table).string());
        if (!t.records->initialize()) return;
        for (const auto& col : t.columns) if (col.indexed) {
            const std::string index_file = tablePath(db, table).string() + "." + col.name + ".idx";
            const bool existed = std::filesystem::exists(index_file) && std::filesystem::file_size(index_file) != 0;
            auto index = std::make_unique<BPlusTree>(tablePath(db, table).string() + "." + col.name, columnType(col));
            if (!index->initialize()) return;
            if (!existed) {
                for (const auto& rid : t.records->scan_all_records()) {
                    auto row = t.records->get_record(rid, makeSchema(t));
                    if (!row.empty() && !row[columnIndex(t, col.name)].is_null()) index->insert(row[columnIndex(t, col.name)], rid);
                }
                index->flush();
            }
            t.indexes[col.name] = std::move(index);
        }
        databases_[db][table] = std::move(t);
    }

    void flushMetadata() {}
};

} // namespace sysdb

#endif // STORAGE_STUB_HPP