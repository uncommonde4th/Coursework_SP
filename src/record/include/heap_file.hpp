#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "page.hpp"
#include "record_id.hpp"

namespace sysdb {

/*
HeapFile - файл с кучей страниц
Обеспечивает:
    - Персистентное хранение страниц на диске
    - Кэширование страниц в памяти (LRU)
    - Потокобезопасность
*/
class HeapFile {
public:
    explicit HeapFile(const std::string& filename);
    ~HeapFile();
    
    // Открытие/закрытие
    bool open();
    void close();
    
    // Основные операции
    RecordId insert(const void* data, uint16_t size);
    bool update(const RecordId& rid, const void* data, uint16_t size);
    bool read_record(const RecordId& rid, void* buffer, uint16_t& size) const;
    bool remove(const RecordId& rid);
    
    // Сканирование
    std::vector<RecordId> scan_all() const;
    
    // Синхронизация
    void flush();
    
    // Статистика
    size_t get_page_count() const { return page_cache_.size(); }
    size_t get_record_count() const;
    
private:
    std::string filename_;
    int fd_;
    mutable std::mutex mutex_;
    uint32_t next_page_id_ = 0;   // счётчик страниц на диске (не зависит от кэша)
    
    // Кэш страниц (простая реализация без LRU для начала)
    mutable std::unordered_map<uint32_t, Page> page_cache_;
    
    // Вспомогательные методы
    bool load_page(uint32_t page_id, Page& page) const;
    bool save_page(uint32_t page_id, const Page& page);
    void load_all_pages();
    Page* get_page(uint32_t page_id);
    const Page* get_page(uint32_t page_id) const;
    uint32_t allocate_new_page();
};

}