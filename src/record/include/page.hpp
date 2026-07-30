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
 * | PageHeader (fixed)|
 * +-------------------+
 * | Slot Directory    |
 * | (MAX_SLOTS * 4)   |    <- фиксированный размер, не входит в data_
 * +-------------------+
 * | data_[DATA_SIZE]  |
 * |  свободно | занято|    <- данные растут от конца data_ к началу
 * +-------------------+ PAGE_SIZE
 *
 * header_.data_start  — индекс внутри data_, начиная с которого данные
 * заняты (данные лежат в диапазоне [data_start, DATA_SIZE)).
 * Соответственно [0, data_start) — непрерывное свободное место.
 * Слот-директория (slots_) — отдельный фиксированный массив, её размер
 * НЕ учитывается при расчёте свободного места внутри data_.
 */

struct PageHeader {
    uint16_t slot_count;        // Количество занятых слотов
    uint16_t data_start;        // Начало занятой области данных внутри data_
    uint16_t next_free_slot;    // Индекс головы списка свободных слотов (0xFFFF - нет)
    uint16_t checksum;          // Контрольная сумма

    PageHeader() : slot_count(0), data_start(0), next_free_slot(0xFFFF), checksum(0) {}
};

struct Slot {
    uint16_t offset;   // Смещение записи внутри data_
    uint16_t length;   // Длина записи (бит 15 = 1, если слот свободен)

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
    static constexpr size_t DATA_SIZE = PAGE_SIZE - sizeof(PageHeader) - MAX_SLOTS * sizeof(Slot);

    Page();
    ~Page() = default;

    // Копирование (все поля - POD, тривиальное почленное копирование корректно)
    Page(const Page& other) = default;
    Page& operator=(const Page& other) = default;

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
    bool is_full() const {
        return header_.slot_count >= MAX_SLOTS || get_free_space() < 64;
    }

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
    char data_[DATA_SIZE];

    // Вспомогательные методы
    uint16_t find_free_slot();
};

static_assert(sizeof(PageHeader) + MAX_SLOTS * sizeof(Slot) + Page::DATA_SIZE == PAGE_SIZE,
              "Page layout must exactly fill PAGE_SIZE");

}
