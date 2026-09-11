#pragma once

#include <memory>
#include <string>
#include <vector>
#include "heap_file.hpp"
#include "record.hpp"
#include "schema.hpp"

namespace sysdb {

/*
RecordManager - высокоуровневый менеджер записей
    Предоставляет удобный интерфейс для работы с записями,
    скрывая детали страниц и сериализации.
*/
class RecordManager {
public:
    explicit RecordManager(const std::string& table_name);
    ~RecordManager();
    
    // Инициализация (создание/открытие файла)
    bool initialize();
    
    // Основные операции
    RecordId insert_record(const std::vector<Value>& values, const TableSchema& schema);
    std::vector<Value> get_record(const RecordId& rid, const TableSchema& schema);
    bool update_record(const RecordId& rid, const std::vector<Value>& values, const TableSchema& schema);
    bool delete_record(const RecordId& rid);
    
    // Сканирование
    std::vector<RecordId> scan_all_records();
    
    // Синхронизация
    void flush();
    
    // Информация
    size_t get_record_count() const { return heap_file_->get_record_count(); }
    const std::string& get_table_name() const { return table_name_; }
    
private:
    std::string table_name_;
    std::unique_ptr<HeapFile> heap_file_;
};

}