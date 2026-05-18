// (временная заглушка)
#pragma once
#include <vector>
#include <string>
#include "value.hpp"

namespace sysdb {

class TableSchema {
public:
    struct Column {
        std::string name;
        Value::Type type;
        bool not_null;
        bool indexed;
        std::string default_value;
    };
    
    void add_column(const std::string& name, Value::Type type, 
                    bool not_null = false, bool indexed = false,
                    const std::string& default_val = "") {
        columns_.push_back({name, type, not_null, indexed, default_val});
    }
    
    std::vector<Value::Type> get_column_types() const {
        std::vector<Value::Type> types;
        for (const auto& col : columns_) {
            types.push_back(col.type);
        }
        return types;
    }
    
    size_t column_count() const { return columns_.size(); }
    const Column& get_column(size_t idx) const { return columns_[idx]; }
    
private:
    std::vector<Column> columns_;
};

}