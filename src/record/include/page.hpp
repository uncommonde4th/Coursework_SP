#pragma once

#include <cstdint>
#include <vector>
#include <cstring>
#include <optional>
#include "record_id.hpp"

namespace sysdb {

constexpr uint32_t PAGE_SIZE = 4096;      // 4KB страница
constexpr uint16_t MAX_SLOTS = 512;       // Максимум слотов на странице

/*
 * Формат страницы:
 * +-------------------+ 0
 * | PageHeader (16)   |
 * +-------------------+ 16
 * | Slot Directory    |
 * | (MAX_SLOTS * 4)   |
 * +-------------------+ 16 + MAX_SLOTS*4
 * | Free Space        |
 * | ...               |
 * +-------------------+ PAGE_SIZE - data_size
 * | Record Data       |
 * | (растет вниз)     |
 * +-------------------+ PAGE_SIZE
 * 
 * Slot Directory: для каждого слота храним (offset, length, flags)
 * offset: смещение от начала страницы (2 байта)
 * length: длина записи (2 байта)
 * flags: 1 бит - свободен ли слот (в старшем бите length)
 */

struct PageHeader {
    uint16_t slot_count;        // Количество использованных слотов
    uint16_t free_space_start;  // Начало свободного места (от начала)
    uint16_t next_free_slot;    // Индекс следующего свободного слота (или 0xFFFF)
    uint16_t checksum;          // Контрольная сумма
    
    PageHeader() : slot_count(0), free_space_start(sizeof(PageHeader) + MAX_SLOTS * 4),
                   next_free_slot(0xFFFF), checksum(0) {}
};

struct Slot {
    uint16_t offset;   // Смещение записи от начала страницы
    uint16_t length;   // Длина записи (бит 15 = 1 если слот свободен)
    
    Slot() : offset(0), length(0x8000) {}  // По умолчанию свободный
    Slot(uint16_t off, uint16_t len) : offset(off), length(len & 0x7FFF) {}
    
    bool is_free() const { return (length & 0x8000) != 0; }
    void set_free(bool free) {
        if (free) length |= 0x8000;
        else length &= 0x7FFF;
    }
    
    uint16_t get_length() const { return length & 0x7FFF; }
    void set_length(uint16_t len) { length = (length & 0x8000) | (len & 0x7FFF); }
};

class Page {
public:
    Page();
    ~Page() = default;
    
    // Копирование
    Page(const Page& other);
    Page& operator=(const Page& other);
    
    // Операции с записями
    bool insert_record(const void* data, uint16_t size, uint16_t& slot_id);
    bool update_record(uint16_t slot_id, const void* data, uint16_t size);
    bool get_record(uint16_t slot_id, const void*& data, uint16_t& size) const;
    bool delete_record(uint16_t slot_id);
    
    // Получение всех RID на странице
    std::vector<RecordId> get_all_record_ids(uint32_t page_id) const;
    
    // Информация о странице
    uint16_t get_free_space() const;
    uint16_t get_slot_count() const { return header_.slot_count; }
    bool is_empty() const { return header_.slot_count == 0; }
    bool is_full() const { return get_free_space() < 64; }  // Меньше 64 байт свободно
    
    // Сериализация/десериализация
    void serialize(char* buffer) const;
    void deserialize(const char* buffer);
    
    // Контрольная сумма
    void update_checksum();
    bool verify_checksum() const;
    
    // Дефрагментация
    void compact();
    
private:
    PageHeader header_;
    Slot slots_[MAX_SLOTS];
    char data_[PAGE_SIZE - sizeof(PageHeader) - MAX_SLOTS * sizeof(Slot)];
    
    // Вспомогательные методы
    uint16_t find_free_slot();
    uint16_t find_best_fit(uint16_t size) const;
    void move_record(uint16_t from_slot, uint16_t to_offset);
};

}