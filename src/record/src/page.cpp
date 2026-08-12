#include "page.hpp"
#include <algorithm>
#include <cassert>

namespace sysdb {

Page::Page() : header_(), slots_{}, data_{} {
    header_.data_start = static_cast<uint16_t>(DATA_SIZE);  // изначально всё свободно
}

bool Page::insert_record(const void* data, uint16_t size, uint16_t& slot_id) {
    if (get_free_space() < size) {
        compact();
        if (get_free_space() < size) {
            return false;
        }
    }

    slot_id = find_free_slot();
    if (slot_id == 0xFFFF) {
        return false;
    }

    header_.data_start -= size;
    memcpy(data_ + header_.data_start, data, size);

    slots_[slot_id].offset = header_.data_start;
    slots_[slot_id].set_length(size);
    slots_[slot_id].set_free(false);

    header_.slot_count++;

    update_checksum();
    return true;
}

bool Page::update_record(uint16_t slot_id, const void* data, uint16_t size) {
    if (slot_id >= MAX_SLOTS || slots_[slot_id].is_free()) {
        return false;
    }

    uint16_t old_size = slots_[slot_id].get_length();

    if (size <= old_size) {
        memcpy(data_ + slots_[slot_id].offset, data, size);
        slots_[slot_id].set_length(size);
    } else {
        if (static_cast<size_t>(get_free_space()) + old_size < size) {
            compact();
            if (get_free_space() < size) {
                return false;
            }
        } else {
            slots_[slot_id].set_free(true);
            header_.slot_count--;

            compact();
        }

        if (get_free_space() < size) {
            return false;
        }

        header_.data_start -= size;
        memcpy(data_ + header_.data_start, data, size);
        slots_[slot_id].offset = header_.data_start;
        slots_[slot_id].set_length(size);
        slots_[slot_id].set_free(false);
        header_.slot_count++;
    }

    update_checksum();
    return true;
}

bool Page::get_record(uint16_t slot_id, const void*& data, uint16_t& size) const {
    if (slot_id >= MAX_SLOTS || slots_[slot_id].is_free()) {
        return false;
    }

    size = slots_[slot_id].get_length();
    data = data_ + slots_[slot_id].offset;
    return true;
}

bool Page::delete_record(uint16_t slot_id) {
    if (slot_id >= MAX_SLOTS || slots_[slot_id].is_free()) {
        return false;
    }

    slots_[slot_id].set_length(0);
    header_.slot_count--;

    // Добавляем слот в голову списка свободных слотов.
    slots_[slot_id].offset = header_.next_free_slot;
    slots_[slot_id].set_free(true);
    header_.next_free_slot = slot_id;

    update_checksum();
    return true;
}

std::vector<RecordId> Page::get_all_record_ids(uint32_t page_id) const {
    std::vector<RecordId> result;
    for (uint16_t i = 0; i < MAX_SLOTS; i++) {
        if (!slots_[i].is_free()) {
            result.emplace_back(page_id, i);
        }
    }
    return result;
}

uint16_t Page::get_free_space() const {
    return header_.data_start;
}

void Page::compact() {
    char new_data[DATA_SIZE] = {};
    uint16_t write_pos = static_cast<uint16_t>(DATA_SIZE);

    for (uint16_t i = 0; i < MAX_SLOTS; i++) {
        if (!slots_[i].is_free()) {
            uint16_t size = slots_[i].get_length();
            write_pos -= size;
            memcpy(new_data + write_pos, data_ + slots_[i].offset, size);
            slots_[i].offset = write_pos;
        }
    }

    memcpy(data_, new_data, DATA_SIZE);
    header_.data_start = write_pos;

    update_checksum();
}

uint16_t Page::find_free_slot() {
    // Сначала пробуем список переиспользуемых (удалённых) слотов.
    if (header_.next_free_slot != 0xFFFF) {
        uint16_t slot = header_.next_free_slot;
        header_.next_free_slot = slots_[slot].offset;
        return slot;
    }

    // Иначе ищем ещё не использованный слот линейно.
    for (uint16_t i = 0; i < MAX_SLOTS; i++) {
        if (slots_[i].is_free() && slots_[i].offset == 0 && slots_[i].get_length() == 0) {
            return i;
        }
    }

    return 0xFFFF;
}

void Page::serialize(char* buffer) const {
    memcpy(buffer, this, PAGE_SIZE);
}

void Page::deserialize(const char* buffer) {
    memcpy(this, buffer, PAGE_SIZE);
}

void Page::update_checksum() {
    header_.checksum = 0;
    uint16_t sum = 0;
    const uint16_t* words = reinterpret_cast<const uint16_t*>(this);
    for (size_t i = 0; i < PAGE_SIZE / 2; i++) {
        sum ^= words[i];
    }
    header_.checksum = sum;
}

bool Page::verify_checksum() const {
    uint16_t stored = header_.checksum;
    Page copy(*this);
    copy.header_.checksum = 0;
    uint16_t sum = 0;
    const uint16_t* words = reinterpret_cast<const uint16_t*>(&copy);
    for (size_t i = 0; i < PAGE_SIZE / 2; i++) {
        sum ^= words[i];
    }
    return sum == stored;
}

}