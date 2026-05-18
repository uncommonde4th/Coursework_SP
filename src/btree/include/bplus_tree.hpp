#pragma once

#include <vector>
#include <map>
#include "record_id.hpp"
#include "value.hpp"

namespace sysdb {

/*
 * B+Tree индекс - заглушка для начальной компиляции
 * Полная реализация будет позже
*/
class BPlusTree {
public:
    BPlusTree(const std::string& index_name, Value::Type key_type);
    ~BPlusTree();
    
    bool initialize();
    void insert(const Value& key, const RecordId& rid);
    void remove(const Value& key, const RecordId& rid);
    std::vector<RecordId> find(const Value& key);
    std::vector<RecordId> range_find(const Value& start, const Value& end);
    
private:
    std::string index_name_;
    Value::Type key_type_;
    
    // Временное хранилище (для тестов)
    std::multimap<Value, RecordId> data_;
};

}