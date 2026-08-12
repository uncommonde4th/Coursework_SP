// (временная заглушка для компиляции)
#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace sysdb {

class Value {
public:
    enum class Type { NULL_VALUE, INT, STRING };
    
    static Value make_int(int64_t val) {
        Value v;
        v.type_ = Type::INT;
        v.int_val_ = val;
        return v;
    }
    
    static Value make_string(const std::string& val) {
        Value v;
        v.type_ = Type::STRING;
        v.string_val_ = val;
        return v;
    }
    
    static Value make_null() {
        Value v;
        v.type_ = Type::NULL_VALUE;
        return v;
    }
    
    Type type() const { return type_; }
    int64_t as_int() const { return int_val_; }
    std::string as_string() const { return string_val_; }
    bool is_null() const { return type_ == Type::NULL_VALUE; }
    
    bool operator==(const Value& other) const {
        if (type_ != other.type_) return false;
        if (type_ == Type::INT) return int_val_ == other.int_val_;
        if (type_ == Type::STRING) return string_val_ == other.string_val_;
        return true;
    }
    
    bool operator<(const Value& other) const {
        if (type_ != other.type_) return false;
        if (type_ == Type::INT) return int_val_ < other.int_val_;
        if (type_ == Type::STRING) return string_val_ < other.string_val_;
        return false;
    }
    
private:
    Type type_ = Type::NULL_VALUE;
    int64_t int_val_ = 0;
    std::string string_val_;
};

}