#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>
#include "value.hpp"

namespace sysdb {

// Максимальная длина строкового ключа индекса. Более длинные строки
// обрезаются - это ограничение зафиксировано
// на уровне ключа индекса и не влияет на хранение самих данных записи
// (там строки хранятся полностью, см. record.hpp).
constexpr size_t MAX_KEY_LEN = 64;

/*
IndexKey - ключ фиксированного размера для узлов B+-дерева.
Нужен, чтобы порядок дерева (число ключей на странице) был константным
и известным на этапе компиляции - это сильно упрощает разбиение/слияние
узлов, т.к. каждый узел занимает ровно одну физическую страницу.
*/
struct IndexKey {
    uint8_t type = 0;               // соответствует Value::Type
    int64_t int_val = 0;            // используется при type == INT
    uint8_t str_len = 0;            // используется при type == STRING
    char str_val[MAX_KEY_LEN] = {}; // используется при type == STRING

    IndexKey() = default;

    static IndexKey from_value(const Value& v) {
        IndexKey k;
        k.type = static_cast<uint8_t>(v.type());
        if (v.type() == Value::Type::INT) {
            k.int_val = v.as_int();
        } else if (v.type() == Value::Type::STRING) {
            std::string s = v.as_string();
            if (s.size() > MAX_KEY_LEN) {
                s = s.substr(0, MAX_KEY_LEN);
            }
            k.str_len = static_cast<uint8_t>(s.size());
            memcpy(k.str_val, s.data(), k.str_len);
        }
        return k;
    }

    Value to_value() const {
        if (type == static_cast<uint8_t>(Value::Type::INT)) {
            return Value::make_int(int_val);
        }
        if (type == static_cast<uint8_t>(Value::Type::STRING)) {
            return Value::make_string(std::string(str_val, str_len));
        }
        return Value::make_null();
    }

    bool operator<(const IndexKey& other) const {
        if (type != other.type) return type < other.type;
        if (type == static_cast<uint8_t>(Value::Type::INT)) {
            return int_val < other.int_val;
        }
        if (type == static_cast<uint8_t>(Value::Type::STRING)) {
            int cmp = memcmp(str_val, other.str_val, std::min(str_len, other.str_len));
            if (cmp != 0) return cmp < 0;
            return str_len < other.str_len;
        }
        return false;
    }

    bool operator==(const IndexKey& other) const {
        if (type != other.type) return false;
        if (type == static_cast<uint8_t>(Value::Type::INT)) {
            return int_val == other.int_val;
        }
        if (type == static_cast<uint8_t>(Value::Type::STRING)) {
            return str_len == other.str_len && memcmp(str_val, other.str_val, str_len) == 0;
        }
        return true;
    }

    bool operator<=(const IndexKey& o) const { return (*this < o) || (*this == o); }
    bool operator>(const IndexKey& o) const { return o < *this; }
    bool operator>=(const IndexKey& o) const { return (o < *this) || (*this == o); }
    bool operator!=(const IndexKey& o) const { return !(*this == o); }
};

}
