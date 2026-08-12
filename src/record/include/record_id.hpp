#pragma once
#include <cstdint>
#include <string>
#include <ostream>
#include <iostream>

namespace sysdb {

struct RecordId {
    uint32_t page_id;
    uint16_t slot_id;
    
    RecordId() : page_id(UINT32_MAX), slot_id(UINT16_MAX) {}
    RecordId(uint32_t page, uint16_t slot) : page_id(page), slot_id(slot) {}
    
    bool operator==(const RecordId& other) const {
        return page_id == other.page_id && slot_id == other.slot_id;
    }
    
    bool operator<(const RecordId& other) const {
        if (page_id != other.page_id) return page_id < other.page_id;
        return slot_id < other.slot_id;
    }
    
    std::string to_string() const {
        return "Page " + std::to_string(page_id) + ", Slot " + std::to_string(slot_id);
    }
    
    
    bool is_valid() const {
        return page_id != UINT32_MAX || slot_id != UINT16_MAX;
    }

    void debug_print() const {
        std::cout << "[RecordId: page=" << page_id << ", slot=" << slot_id << "]" << std::endl;
    }
};

inline std::ostream& operator<<(std::ostream& os, const RecordId& rid) {
    os << "P" << rid.page_id << "S" << rid.slot_id;
    return os;
}

}