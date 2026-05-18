// src/record/record.hpp
#pragma once

#include <vector>
#include <string>
#include <cstring>
#include "value.hpp"  // Будет реализован позже, пока заглушка

namespace sysdb {

/**
 * Сериализация: Value[] → бинарный буфер
 * 
 * Формат:
 * [count: 2 байта]
 * [type_flags: 1 байт на поле] (бит 0: is_null, биты 1-2: тип)
 * [данные: переменная длина]
 *   - int: 8 байт
 *   - string: 2 байта длина + данные
 */
inline std::vector<char> serialize_record(const std::vector<Value>& values) {
    // TODO: полная реализация после создания Value
    std::vector<char> result;
    
    // Временная заглушка для компиляции
    result.resize(1024);
    return result;
}

// Десериализация
inline std::vector<Value> deserialize_record(const std::vector<char>& data, 
                                              const std::vector<Value::Type>& column_types) {
    // TODO: полная реализация после создания Value
    std::vector<Value> result;
    for (const auto& type : column_types) {
        if (type == Value::Type::INT) {
            result.push_back(Value::make_int(0));
        } else {
            result.push_back(Value::make_string(""));
        }
    }
    return result;
}

}