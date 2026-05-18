#include "page.hpp"
#include <algorithm>
#include <cassert>

namespace sysdb {

Page::Page() {
    memset(this, 0, sizeof(Page));
    header_.free_space_start = sizeof(PageHeader) + MAX_SLOTS * sizeof(Slot);
    header_.next_free_slot = 0xFFFF;
    
    // Инициализируем все слоты как свободные
    for (auto& slot : slots_) {
        slot.set_free(true);
        slot.offset = 0;
        slot.set_length(0);
    }
}

Page::Page(const Page& other) {
    memcpy(this, &other, sizeof(Page));
}

Page& Page::operator=(const Page& other) {
    if (this != &other) {
        memcpy(this, &other, sizeof(Page));
    }
    return *this;
}

bool Page::insert_record(const void* data, uint16_t size, uint16_t& slot_id) {
    // Проверяем, есть ли свободное место
    uint16_t required_space = size + sizeof(Slot);
    if (get_free_space() < required_space) {
        compact();
        if (get_free_space() < required_space) {
            return false;
        }
    }
    
    // Находим свободный слот
    slot_id = find_free_slot();
    if (slot_id == 0xFFFF) {
        return false;
    }
    
    // Вычисляем позицию для вставки (с конца свободного места)
    uint16_t insert_pos = header_.free_space_start;
    // Запись растет вниз от free_space_start к концу страницы
    // Но данные вставляются с конца, поэтому нужно сдвигать
    uint16_t data_pos = PAGE_SIZE - header_.free_space_start;
    data_pos -= size;
    
    // Копируем данные
    memcpy(data_ + data_pos, data, size);
    
    // Заполняем слот
    slots_[slot_id].offset = data_pos;
    slots_[slot_id].set_length(size);
    slots_[slot_id].set_free(false);
    
    header_.free_space_start += size + sizeof(Slot);
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
        // Новая запись меньше или равна старой - просто перезаписываем
        memcpy(data_ + slots_[slot_id].offset, data, size);
        
        // Если новая запись меньше, можно освободить место
        if (size < old_size) {
            slots_[slot_id].set_length(size);
            header_.free_space_start -= (old_size - size);
        }
    } else {
        // Новая запись больше - нужно перевставить
        uint16_t new_slot_id;
        if (!insert_record(data, size, new_slot_id)) {
            return false;
        }
        
        // Копируем старый слот в новый и удаляем старый
        slots_[new_slot_id] = slots_[slot_id];
        delete_record(slot_id);
        
        // Обновляем ссылку для вызывающего кода
        slot_id = new_slot_id;
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
    
    // Помечаем слот как свободный
    slots_[slot_id].set_free(true);
    slots_[slot_id].set_length(0);
    slots_[slot_id].offset = 0;
    
    header_.slot_count--;
    
    // Добавляем в список свободных слотов (простая реализация)
    slots_[slot_id].offset = header_.next_free_slot;
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
    return PAGE_SIZE - header_.free_space_start - header_.slot_count * sizeof(Slot);
}

void Page::compact() {
    // Дефрагментация: двигаем все живые записи в конец страницы
    char new_data[PAGE_SIZE - sizeof(PageHeader) - MAX_SLOTS * sizeof(Slot)];
    uint16_t current_pos = sizeof(new_data);
    
    // Обновляем смещения для живых записей
    for (uint16_t i = 0; i < MAX_SLOTS; i++) {
        if (!slots_[i].is_free()) {
            uint16_t size = slots_[i].get_length();
            current_pos -= size;
            memcpy(new_data + current_pos, data_ + slots_[i].offset, size);
            slots_[i].offset = current_pos;
        }
    }
    
    // Копируем обратно
    memcpy(data_, new_data, sizeof(new_data));
    
    // Обновляем указатель свободного места
    header_.free_space_start = sizeof(PageHeader) + MAX_SLOTS * sizeof(Slot) + 
                               (MAX_SLOTS - header_.slot_count) * sizeof(Slot);
    
    update_checksum();
}

uint16_t Page::find_free_slot() {
    // Сначала проверяем список свободных слотов
    if (header_.next_free_slot != 0xFFFF) {
        uint16_t slot = header_.next_free_slot;
        header_.next_free_slot = slots_[slot].offset;
        return slot;
    }
    
    // Ищем линейно
    for (uint16_t i = 0; i < MAX_SLOTS; i++) {
        if (slots_[i].is_free()) {
            return i;
        }
    }
    
    return 0xFFFF;  // Не найдено
}

void Page::serialize(char* buffer) const {
    memcpy(buffer, this, PAGE_SIZE);
}

void Page::deserialize(const char* buffer) {
    memcpy(this, buffer, PAGE_SIZE);
}

void Page::update_checksum() {
    uint16_t sum = 0;
    const uint16_t* words = reinterpret_cast<const uint16_t*>(this);
    for (size_t i = 0; i < PAGE_SIZE / 2; i++) {
        sum ^= words[i];
    }
    header_.checksum = sum;
}

bool Page::verify_checksum() const {
    uint16_t sum = 0;
    const uint16_t* words = reinterpret_cast<const uint16_t*>(this);
    for (size_t i = 0; i < PAGE_SIZE / 2; i++) {
        sum ^= words[i];
    }
    return sum == header_.checksum;
}

}