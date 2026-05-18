#include "bplus_tree.hpp"

namespace sysdb {

BPlusTree::BPlusTree(const std::string& index_name, Value::Type key_type)
    : index_name_(index_name), key_type_(key_type) {}

BPlusTree::~BPlusTree() {}

bool BPlusTree::initialize() {
    // TODO: создать файл индекса
    return true;
}

void BPlusTree::insert(const Value& key, const RecordId& rid) {
    data_.insert({key, rid});
}

void BPlusTree::remove(const Value& key, const RecordId& rid) {
    auto range = data_.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == rid) {
            data_.erase(it);
            break;
        }
    }
}

std::vector<RecordId> BPlusTree::find(const Value& key) {
    std::vector<RecordId> result;
    auto range = data_.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        result.push_back(it->second);
    }
    return result;
}

std::vector<RecordId> BPlusTree::range_find(const Value& start, const Value& end) {
    std::vector<RecordId> result;
    auto it = data_.lower_bound(start);
    while (it != data_.end() && it->first < end) {
        result.push_back(it->second);
        ++it;
    }
    return result;
}

}