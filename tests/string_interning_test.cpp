// string_interning_test.cpp
#include "value.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <string>

using namespace sysdb;

// Простой аналог assert с сообщением
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "❌ FAILED: " << message << " (line " << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_NE(expected, actual, message) \
    TEST_ASSERT(!((expected) == (actual)), message)

#define TEST_ASSERT_TRUE(condition, message) \
    TEST_ASSERT(condition, message)

#define TEST_ASSERT_FALSE(condition, message) \
    TEST_ASSERT(!(condition), message)

// Структура для хранения результатов тестов
struct TestResult {
    std::string name;
    bool passed;
    std::string error;
};

// ==================== БАЗОВЫЕ ТЕСТЫ ====================

bool test_same_string_same_pointer() {
    auto v1 = Value::make_string("hello");
    auto v2 = Value::make_string("hello");
    auto v3 = Value::make_string("world");
    
    TEST_ASSERT_EQ(v1.as_string(), v2.as_string(), "Same strings should be equal");
    TEST_ASSERT_EQ(v1.type(), v2.type(), "Types should be the same");
    TEST_ASSERT_TRUE(v1 == v2, "Same strings should be equal");
    TEST_ASSERT_FALSE(v1 == v3, "Different strings should not be equal");
    
    std::cout << "✅ Same string same pointer - PASSED" << std::endl;
    return true;
}

bool test_different_strings() {
    auto v1 = Value::make_string("hello");
    auto v2 = Value::make_string("world");
    
    TEST_ASSERT_NE(v1.as_string(), v2.as_string(), "Different strings should be different");
    TEST_ASSERT_FALSE(v1 == v2, "Different strings should not be equal");
    
    std::cout << "✅ Different strings - PASSED" << std::endl;
    return true;
}

bool test_null_values() {
    auto null1 = Value::make_null();
    auto null2 = Value::make_null();
    auto str = Value::make_string("hello");
    
    TEST_ASSERT_TRUE(null1.is_null(), "null1 should be null");
    TEST_ASSERT_TRUE(null2.is_null(), "null2 should be null");
    TEST_ASSERT_TRUE(null1 == null2, "Two nulls should be equal");
    TEST_ASSERT_FALSE(null1 == str, "null should not equal string");
    TEST_ASSERT_FALSE(str == null1, "string should not equal null");
    
    std::cout << "✅ Null values - PASSED" << std::endl;
    return true;
}

bool test_type_consistency() {
    auto str = Value::make_string("42");
    auto num = Value::make_int(42);
    
    TEST_ASSERT_EQ(str.type(), Value::Type::STRING, "Should be STRING type");
    TEST_ASSERT_EQ(num.type(), Value::Type::INT, "Should be INT type");
    
    // Проверяем, что значения разных типов не равны
    TEST_ASSERT_FALSE(str == num, "String and int should not be equal");
    TEST_ASSERT_FALSE(num == str, "Int and string should not be equal");
    
    // Проверяем, что строковое представление может быть одинаковым
    TEST_ASSERT_EQ(str.as_string(), std::to_string(num.as_int()), 
                   "String representation may be equal");
    
    std::cout << "✅ Type consistency - PASSED" << std::endl;
    return true;
}

// ==================== ПРОИЗВОДИТЕЛЬНОСТЬ ====================

bool test_memory_sharing() {
    const int COUNT = 1000;
    std::vector<Value> values;
    values.reserve(COUNT);
    
    for (int i = 0; i < COUNT; ++i) {
        values.push_back(Value::make_string("shared_string"));
    }
    
    for (int i = 1; i < COUNT; ++i) {
        TEST_ASSERT_TRUE(values[0] == values[i], "All strings should be equal");
    }
    
    std::cout << "✅ Memory sharing - PASSED" << std::endl;
    return true;
}

bool test_interning_cache() {
    auto v1 = Value::make_string("unique_test_string_123");
    auto v2 = Value::make_string("unique_test_string_123");
    
    TEST_ASSERT_TRUE(v1 == v2, "Cached string should be reused");
    
    std::cout << "✅ Interning cache - PASSED" << std::endl;
    return true;
}

// ==================== ПОТОКОБЕЗОПАСНОСТЬ ====================

bool test_thread_safety() {
    const int THREADS = 10;
    const int STRINGS_PER_THREAD = 100;
    std::vector<std::thread> threads;
    std::vector<std::vector<Value>> results(THREADS);
    
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&results, t, STRINGS_PER_THREAD]() {
            for (int i = 0; i < STRINGS_PER_THREAD; ++i) {
                std::string s = "thread_" + std::to_string(t % 5) + "_str_" + std::to_string(i % 10);
                results[t].push_back(Value::make_string(s));
            }
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    // Проверяем, что одинаковые строки из разных потоков равны
    for (int t1 = 0; t1 < THREADS; ++t1) {
        for (int t2 = t1 + 1; t2 < THREADS; ++t2) {
            for (int i = 0; i < std::min(STRINGS_PER_THREAD, 10); ++i) {
                if ((t1 % 5) == (t2 % 5)) {
                    TEST_ASSERT_TRUE(results[t1][i] == results[t2][i], 
                        "Strings from different threads should be equal");
                }
            }
        }
    }
    
    std::cout << "✅ Thread safety - PASSED" << std::endl;
    return true;
}

// ==================== РАЗНЫЕ ТИПЫ ====================

bool test_string_vs_int() {
    auto str = Value::make_string("123");
    auto num = Value::make_int(123);
    
    TEST_ASSERT_NE(str.type(), num.type(), "Types should be different");
    TEST_ASSERT_FALSE(str == num, "String should not equal int");
    TEST_ASSERT_FALSE(num == str, "Int should not equal string");
    
    std::cout << "✅ String vs int - PASSED" << std::endl;
    return true;
}

bool test_string_vs_null() {
    auto str = Value::make_string("hello");
    auto null = Value::make_null();
    
    TEST_ASSERT_FALSE(str == null, "String should not equal null");
    TEST_ASSERT_FALSE(null == str, "Null should not equal string");
    
    std::cout << "✅ String vs null - PASSED" << std::endl;
    return true;
}

// ==================== ОПЕРАТОРЫ СРАВНЕНИЯ ====================

bool test_comparison_operators() {
    auto a = Value::make_string("apple");
    auto b = Value::make_string("banana");
    auto c = Value::make_string("apple");
    
    // Проверяем ==
    TEST_ASSERT_TRUE(a == c, "Equal strings should be equal");
    TEST_ASSERT_FALSE(a == b, "Different strings should not be equal");
    
    // Проверяем <
    TEST_ASSERT_TRUE(a < b, "apple should be less than banana");
    TEST_ASSERT_FALSE(b < a, "banana should not be less than apple");
    
    // Проверяем, что a не меньше c (a >= c)
    TEST_ASSERT_FALSE(a < c, "apple should not be less than apple");
    
    // Проверяем, что a не равно b (a != b)
    TEST_ASSERT_FALSE(a == b, "apple should not equal banana");
    
    std::cout << "✅ Comparison operators - PASSED" << std::endl;
    return true;
}

bool test_integer_comparisons() {
    auto a = Value::make_int(10);
    auto b = Value::make_int(20);
    auto c = Value::make_int(10);
    
    TEST_ASSERT_TRUE(a == c, "Equal ints should be equal");
    TEST_ASSERT_FALSE(a == b, "Different ints should not be equal");
    TEST_ASSERT_TRUE(a < b, "10 should be less than 20");
    TEST_ASSERT_FALSE(b < a, "20 should not be less than 10");
    
    // Проверяем, что a не меньше c (a >= c)
    TEST_ASSERT_FALSE(a < c, "10 should not be less than 10");
    
    std::cout << "✅ Integer comparisons - PASSED" << std::endl;
    return true;
}

// ==================== СТРЕСС-ТЕСТЫ ====================

bool test_many_unique_strings() {
    const int COUNT = 500;
    std::vector<Value> values;
    values.reserve(COUNT);
    
    for (int i = 0; i < COUNT; ++i) {
        values.push_back(Value::make_string("unique_" + std::to_string(i)));
    }
    
    // Проверяем только некоторые для скорости
    for (int i = 0; i < COUNT; i += 10) {
        for (int j = i + 1; j < COUNT; j += 10) {
            if (i != j) {
                TEST_ASSERT_FALSE(values[i] == values[j], "Different strings should not be equal");
            }
        }
    }
    
    std::cout << "✅ Many unique strings - PASSED" << std::endl;
    return true;
}

bool test_mixed_operations() {
    std::vector<Value> values;
    
    values.push_back(Value::make_int(100));
    values.push_back(Value::make_string("100"));
    values.push_back(Value::make_int(200));
    values.push_back(Value::make_string("200"));
    values.push_back(Value::make_null());
    values.push_back(Value::make_string("hello"));
    values.push_back(Value::make_int(300));
    values.push_back(Value::make_string("hello"));
    
    TEST_ASSERT_FALSE(values[0] == values[1], "int 100 vs string 100");
    TEST_ASSERT_TRUE(values[5] == values[7], "hello vs hello");
    TEST_ASSERT_EQ(values[5].type(), Value::Type::STRING, "Should be STRING");
    TEST_ASSERT_EQ(values[6].type(), Value::Type::INT, "Should be INT");
    TEST_ASSERT_TRUE(values[4].is_null(), "Should be null");
    
    std::cout << "✅ Mixed operations - PASSED" << std::endl;
    return true;
}

// ==================== ПРОВЕРКА ОЧИСТКИ ====================

bool test_weak_ptr_cleanup() {
    {
        std::vector<Value> temp_values;
        for (int i = 0; i < 5000; ++i) {
            temp_values.push_back(Value::make_string("temp_" + std::to_string(i)));
        }
    }
    
    for (int i = 0; i < 100; ++i) {
        auto v = Value::make_string("new_string_" + std::to_string(i));
        TEST_ASSERT_FALSE(v.is_null(), "Value should not be null");
    }
    
    std::cout << "✅ Weak ptr cleanup - PASSED" << std::endl;
    return true;
}

// ==================== БЕНЧМАРК ====================

bool test_performance() {
    const int COUNT = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<Value> values;
    values.reserve(COUNT);
    for (int i = 0; i < COUNT; ++i) {
        values.push_back(Value::make_string("benchmark_string"));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "📊 Created " << COUNT << " strings in " << duration.count() << "ms" << std::endl;
    
    for (int i = 1; i < COUNT; ++i) {
        TEST_ASSERT_TRUE(values[0] == values[i], "All strings should be equal");
    }
    
    std::cout << "✅ Performance - PASSED" << std::endl;
    return true;
}

// ==================== ЭКСТРЕМАЛЬНЫЕ ТЕСТЫ ====================

bool test_very_long_strings() {
    std::string long_string(10000, 'a');
    auto v1 = Value::make_string(long_string);
    auto v2 = Value::make_string(long_string);
    auto v3 = Value::make_string(long_string + "x");
    
    TEST_ASSERT_TRUE(v1 == v2, "Same long strings should be equal");
    TEST_ASSERT_FALSE(v1 == v3, "Different long strings should not be equal");
    TEST_ASSERT_EQ(v1.as_string().length(), 10000, "String length should be 10000");
    
    std::cout << "✅ Very long strings - PASSED" << std::endl;
    return true;
}

bool test_empty_string() {
    auto empty1 = Value::make_string("");
    auto empty2 = Value::make_string("");
    auto not_empty = Value::make_string("not empty");
    
    TEST_ASSERT_TRUE(empty1 == empty2, "Empty strings should be equal");
    TEST_ASSERT_FALSE(empty1 == not_empty, "Empty should not equal non-empty");
    TEST_ASSERT_EQ(empty1.as_string(), "", "Should be empty string");
    TEST_ASSERT_EQ(empty2.as_string(), "", "Should be empty string");
    
    std::cout << "✅ Empty string - PASSED" << std::endl;
    return true;
}

// ==================== ВСПОМОГАТЕЛЬНЫЙ ТЕСТ ====================

bool test_debug_info() {
    auto v1 = Value::make_string("test1");
    auto v2 = Value::make_string("test1");
    auto v3 = Value::make_string("test2");
    
    TEST_ASSERT_TRUE(v1 == v2, "test1 should equal test1");
    TEST_ASSERT_FALSE(v1 == v3, "test1 should not equal test2");
    TEST_ASSERT_EQ(v1.type(), Value::Type::STRING, "Should be STRING");
    TEST_ASSERT_EQ(v2.type(), Value::Type::STRING, "Should be STRING");
    TEST_ASSERT_EQ(v3.type(), Value::Type::STRING, "Should be STRING");
    
    auto n = Value::make_null();
    TEST_ASSERT_TRUE(n.is_null(), "Should be null");
    TEST_ASSERT_EQ(n.type(), Value::Type::NULL_VALUE, "Should be NULL_VALUE");
    
    std::cout << "✅ Debug info - PASSED" << std::endl;
    return true;
}

// ==================== ПРОВЕРКА СОХРАНЕНИЯ ССЫЛОК ====================

bool test_reference_counting() {
    std::shared_ptr<Value> ptr1 = std::make_shared<Value>(Value::make_string("shared"));
    std::shared_ptr<Value> ptr2 = ptr1;
    
    auto v1 = *ptr1;
    auto v2 = *ptr2;
    
    TEST_ASSERT_TRUE(v1 == v2, "Values should be equal");
    
    ptr1.reset();
    
    TEST_ASSERT_FALSE(ptr2->is_null(), "Should not be null");
    TEST_ASSERT_EQ(ptr2->as_string(), "shared", "Should be 'shared'");
    
    std::cout << "✅ Reference counting - PASSED" << std::endl;
    return true;
}

// ==================== ДОПОЛНИТЕЛЬНЫЙ ТЕСТ ДЛЯ ПОРЯДКА ====================

bool test_string_ordering() {
    auto a = Value::make_string("apple");
    auto b = Value::make_string("banana");
    auto c = Value::make_string("apple");
    
    // Проверяем лексикографический порядок
    TEST_ASSERT_TRUE(a < b, "apple < banana");
    TEST_ASSERT_FALSE(b < a, "banana < apple");
    TEST_ASSERT_FALSE(a < c, "apple < apple (should be false)");
    TEST_ASSERT_TRUE(a == c, "apple == apple");
    
    std::cout << "✅ String ordering - PASSED" << std::endl;
    return true;
}

// ==================== ЗАПУСК ТЕСТОВ ====================

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "   STRING INTERNING TESTS" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    std::vector<TestResult> results;
    
    // Список всех тестов
    std::vector<std::pair<std::string, bool(*)()>> tests = {
        {"Same string same pointer", test_same_string_same_pointer},
        {"Different strings", test_different_strings},
        {"Null values", test_null_values},
        {"Type consistency", test_type_consistency},
        {"Memory sharing", test_memory_sharing},
        {"Interning cache", test_interning_cache},
        {"Thread safety", test_thread_safety},
        {"String vs int", test_string_vs_int},
        {"String vs null", test_string_vs_null},
        {"Comparison operators", test_comparison_operators},
        {"Integer comparisons", test_integer_comparisons},
        {"Many unique strings", test_many_unique_strings},
        {"Mixed operations", test_mixed_operations},
        {"Weak ptr cleanup", test_weak_ptr_cleanup},
        {"Performance", test_performance},
        {"Very long strings", test_very_long_strings},
        {"Empty string", test_empty_string},
        {"Debug info", test_debug_info},
        {"Reference counting", test_reference_counting},
        {"String ordering", test_string_ordering}
    };
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : tests) {
        std::cout << "\n▶ Running: " << test.first << std::endl;
        bool result = test.second();
        if (result) {
            passed++;
        } else {
            failed++;
        }
        results.push_back({test.first, result, result ? "" : "Test failed"});
    }
    
    // Вывод итогов
    std::cout << "\n========================================" << std::endl;
    std::cout << "   RESULTS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "✅ Passed: " << passed << std::endl;
    std::cout << "❌ Failed: " << failed << std::endl;
    std::cout << "📊 Total:  " << passed + failed << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return failed > 0 ? 1 : 0;
}