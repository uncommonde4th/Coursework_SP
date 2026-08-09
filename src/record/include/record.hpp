#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include "value.hpp"

namespace sysdb {

/*
Сериализация: Value[] -> бинарный буфер

Формат:
[count: 2 байта, uint16_t]
далее для каждого поля:
  [flag: 1 байт]  бит 0 - is_null; биты 1-2 - тип (0=NULL,1=INT,2=STRING)
  если не NULL:
   INT:    [8 байт, int64_t]
   STRING: [2 байта длина, uint16_t][len байт данных]
*/
inline std::vector<char> serialize_record(const std::vector<Value>& values) {
    std::vector<char> result;

    uint16_t count = static_cast<uint16_t>(values.size());
    result.resize(sizeof(uint16_t));
    memcpy(result.data(), &count, sizeof(uint16_t));

    for (const auto& v : values) {
        uint8_t flag = static_cast<uint8_t>(v.type()) << 1;
        if (v.is_null()) flag |= 0x01;

        result.push_back(static_cast<char>(flag));

        if (v.is_null()) {
            continue;
        }

        if (v.type() == Value::Type::INT) {
            int64_t iv = v.as_int();
            size_t offset = result.size();
            result.resize(offset + sizeof(int64_t));
            memcpy(result.data() + offset, &iv, sizeof(int64_t));
        } else if (v.type() == Value::Type::STRING) {
            std::string sv = v.as_string();
            uint16_t len = static_cast<uint16_t>(sv.size());
            size_t offset = result.size();
            result.resize(offset + sizeof(uint16_t) + len);
            memcpy(result.data() + offset, &len, sizeof(uint16_t));
            memcpy(result.data() + offset + sizeof(uint16_t), sv.data(), len);
        }
    }

    return result;
}

// Десериализация
inline std::vector<Value> deserialize_record(const std::vector<char>& data,
                                              const std::vector<Value::Type>& column_types) {
    std::vector<Value> result;
    size_t pos = 0;

    auto need = [&](size_t bytes) {
        if (pos + bytes > data.size()) {
            throw std::runtime_error("deserialize_record: corrupted buffer (out of bounds)");
        }
    };

    need(sizeof(uint16_t));
    uint16_t count;
    memcpy(&count, data.data() + pos, sizeof(uint16_t));
    pos += sizeof(uint16_t);

    result.reserve(count);

    for (uint16_t i = 0; i < count; i++) {
        need(1);
        uint8_t flag = static_cast<uint8_t>(data[pos]);
        pos += 1;

        bool is_null = (flag & 0x01) != 0;
        auto type = static_cast<Value::Type>(flag >> 1);

        if (is_null) {
            result.push_back(Value::make_null());
            continue;
        }

        if (type == Value::Type::INT) {
            need(sizeof(int64_t));
            int64_t iv;
            memcpy(&iv, data.data() + pos, sizeof(int64_t));
            pos += sizeof(int64_t);
            result.push_back(Value::make_int(iv));
        } else if (type == Value::Type::STRING) {
            need(sizeof(uint16_t));
            uint16_t len;
            memcpy(&len, data.data() + pos, sizeof(uint16_t));
            pos += sizeof(uint16_t);
            need(len);
            result.push_back(Value::make_string(std::string(data.data() + pos, len)));
            pos += len;
        } else {
            result.push_back(Value::make_null());
        }
    }

    (void)column_types;

    return result;
}

}
