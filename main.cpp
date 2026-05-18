#include <iostream>
#include <string>
#include <memory>
#include "heap_file.hpp"
#include "record_manager.hpp"
#include "bplus_tree.hpp"
#include "value.hpp"
#include "schema.hpp"

using namespace sysdb;

void print_separator() {
    std::cout << "========================================" << std::endl;
}

void test_heap_file() {
    std::cout << "\n=== Testing HeapFile ===" << std::endl;
    
    HeapFile hf("test.dat");
    if (!hf.open()) {
        std::cerr << "Failed to open test.dat" << std::endl;
        return;
    }
    
    // Вставляем записи
    const char* data1 = "Hello World";
    const char* data2 = "Test Record 123";
    const char* data3 = "Another record";
    
    auto rid1 = hf.insert(data1, strlen(data1) + 1);
    auto rid2 = hf.insert(data2, strlen(data2) + 1);
    auto rid3 = hf.insert(data3, strlen(data3) + 1);
    
    std::cout << "Inserted records:" << std::endl;
    std::cout << "  Record 1: " << rid1.to_string() << std::endl;
    std::cout << "  Record 2: " << rid2.to_string() << std::endl;
    std::cout << "  Record 3: " << rid3.to_string() << std::endl;
    
    // Читаем записи
    char buffer[256];
    uint16_t size;
    
    if (hf.read_record(rid1, buffer, size)) {
        std::cout << "  Read record 1: " << buffer << std::endl;
    }
    
    if (hf.read_record(rid2, buffer, size)) {
        std::cout << "  Read record 2: " << buffer << std::endl;
    }
    
    // Сканируем все записи
    auto all_rids = hf.scan_all();
    std::cout << "Total records: " << all_rids.size() << std::endl;
    
    hf.close();
}

void test_record_manager() {
    std::cout << "\n=== Testing RecordManager ===" << std::endl;
    
    // Создаем схему таблицы
    TableSchema schema;
    schema.add_column("id", Value::Type::INT, true, true);
    schema.add_column("name", Value::Type::STRING, false, false);
    schema.add_column("age", Value::Type::INT, false, false);
    
    RecordManager rm("users");
    if (!rm.initialize()) {
        std::cerr << "Failed to initialize RecordManager" << std::endl;
        return;
    }
    
    // Вставляем записи
    std::vector<Value> record1 = {
        Value::make_int(1),
        Value::make_string("Alice"),
        Value::make_int(25)
    };
    
    std::vector<Value> record2 = {
        Value::make_int(2),
        Value::make_string("Bob"),
        Value::make_int(30)
    };
    
    auto rid1 = rm.insert_record(record1, schema);
    auto rid2 = rm.insert_record(record2, schema);
    
    std::cout << "Inserted records via RecordManager:" << std::endl;
    std::cout << "  Record 1: " << rid1.to_string() << std::endl;
    std::cout << "  Record 2: " << rid2.to_string() << std::endl;
    
    // Читаем записи
    auto read1 = rm.get_record(rid1, schema);
    if (!read1.empty()) {
        std::cout << "  Read record 1: id=" << read1[0].as_int() 
                  << ", name=" << read1[1].as_string() 
                  << ", age=" << read1[2].as_int() << std::endl;
    }
    
    // Сканируем все
    auto all_rids = rm.scan_all_records();
    std::cout << "Total records in table: " << all_rids.size() << std::endl;
    std::cout << "Total record count: " << rm.get_record_count() << std::endl;
    
    rm.flush();
}

void test_bplus_tree() {
    std::cout << "\n=== Testing BPlusTree ===" << std::endl;
    
    BPlusTree tree("test_index", Value::Type::INT);
    
    // Вставляем значения
    tree.insert(Value::make_int(5), RecordId(0, 0));
    tree.insert(Value::make_int(3), RecordId(0, 1));
    tree.insert(Value::make_int(7), RecordId(0, 2));
    tree.insert(Value::make_int(5), RecordId(1, 0));  // Дубликат
    
    // Ищем
    auto found = tree.find(Value::make_int(5));
    std::cout << "Found " << found.size() << " records with key 5" << std::endl;
    for (const auto& rid : found) {
        std::cout << "  " << rid.to_string() << std::endl;
    }
    
    // Ищем несуществующий
    auto not_found = tree.find(Value::make_int(10));
    std::cout << "Found " << not_found.size() << " records with key 10" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== SysDB Starting ===" << std::endl;
    std::cout << "Version 1.0" << std::endl;
    print_separator();
    
    // Запускаем тесты
    test_heap_file();
    print_separator();
    
    test_record_manager();
    print_separator();
    
    test_bplus_tree();
    print_separator();
    
    std::cout << "\n=== SysDB Shutdown ===" << std::endl;
    
    return 0;
}