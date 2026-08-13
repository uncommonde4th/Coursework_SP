#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

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
        v.string_val_ = intern(val);
        return v;
    }
    
    static Value make_null() {
        Value v;
        v.type_ = Type::NULL_VALUE;
        return v;
    }
    
    Type type() const { return type_; }
    int64_t as_int() const { return int_val_; }
    std::string as_string() const { return string_val_ ? *string_val_ : std::string(); }
    bool is_null() const { return type_ == Type::NULL_VALUE; }
    
    bool operator==(const Value& other) const {
        if (type_ != other.type_) return false;
        if (type_ == Type::INT) return int_val_ == other.int_val_;
        if (type_ == Type::STRING) {
            if (string_val_ == other.string_val_) return true; // одна и та же интернированная строка
            if (!string_val_ || !other.string_val_) return false;
            return *string_val_ == *other.string_val_;
        }
        return true;
    }
    
    bool operator<(const Value& other) const {
        if (type_ != other.type_) return false;
        if (type_ == Type::INT) return int_val_ < other.int_val_;
        if (type_ == Type::STRING) {
            if (!string_val_ || !other.string_val_) return false;
            return *string_val_ < *other.string_val_;
        }
        return false;
    }
    
private:
    Type type_ = Type::NULL_VALUE;
    int64_t int_val_ = 0;
    /* Доп. задание 2: String Interning / Deduplication.
    Строковое значение хранится не "по значению", а через shared_ptr на
    единственный в процессе экземпляр std::string, найденный/созданный в
    общем пуле intern(). Все Value с одинаковым текстом (в любой таблице)
    разделяют один и тот же блок памяти под сами символы - копируется
    только счётчик ссылок shared_ptr, а не данные строки. */
    std::shared_ptr<const std::string> string_val_;

    static std::shared_ptr<const std::string> intern(const std::string& s) {
        static std::mutex mtx;
        static std::unordered_map<std::string, std::weak_ptr<const std::string>> pool;
        static size_t inserts_since_cleanup = 0;

        std::lock_guard<std::mutex> lock(mtx);

        auto it = pool.find(s);
        if (it != pool.end()) {
            if (auto existing = it->second.lock()) {
                return existing; // строка уже есть в памяти - переиспользуем
            }
        }

        auto ptr = std::make_shared<const std::string>(s);
        pool[s] = ptr;

        // Периодически убираем никем не используемые записи.
        if (++inserts_since_cleanup >= 4096) {
            inserts_since_cleanup = 0;
            for (auto pit = pool.begin(); pit != pool.end();) {
                if (pit->second.expired()) pit = pool.erase(pit);
                else ++pit;
            }
        }

        return ptr;
    }
};

}
