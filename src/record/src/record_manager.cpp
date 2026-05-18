#include "record_manager.hpp"

namespace sysdb {

RecordManager::RecordManager(const std::string& table_name) 
    : table_name_(table_name) {}

RecordManager::~RecordManager() {
    flush();
}

bool RecordManager::initialize() {
    heap_file_ = std::make_unique<HeapFile>(table_name_ + ".dat");
    return heap_file_->open();
}

RecordId RecordManager::insert_record(const std::vector<Value>& values, const TableSchema& schema) {
    std::vector<char> serialized = serialize_record(values);
    return heap_file_->insert(serialized.data(), serialized.size());
}

std::vector<Value> RecordManager::get_record(const RecordId& rid, const TableSchema& schema) {
    char buffer[4096];
    uint16_t size;
    
    if (!heap_file_->read_record(rid, buffer, size)) {
        return {};
    }
    
    std::vector<char> data(buffer, buffer + size);
    return deserialize_record(data, schema.get_column_types());
}

bool RecordManager::update_record(const RecordId& rid, const std::vector<Value>& values, const TableSchema& schema) {
    std::vector<char> serialized = serialize_record(values);
    return heap_file_->update(rid, serialized.data(), serialized.size());
}

bool RecordManager::delete_record(const RecordId& rid) {
    return heap_file_->remove(rid);
}

std::vector<RecordId> RecordManager::scan_all_records() {
    return heap_file_->scan_all();
}

void RecordManager::flush() {
    if (heap_file_) {
        heap_file_->flush();
    }
}

}