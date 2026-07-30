#include "heap_file.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cassert>
#include <algorithm>

namespace sysdb {

HeapFile::HeapFile(const std::string& filename) 
    : filename_(filename), fd_(-1) {}

HeapFile::~HeapFile() {
    close();
}

bool HeapFile::open() {
    fd_ = ::open(filename_.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd_ < 0) {
        return false;
    }
    
    load_all_pages();
    return true;
}

void HeapFile::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (fd_ >= 0) {
        flush();
        ::close(fd_);
        fd_ = -1;
    }
    page_cache_.clear();
}

RecordId HeapFile::insert(const void* data, uint16_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Ищем страницу со свободным местом
    for (auto& [page_id, page] : page_cache_) {
        uint16_t slot_id;
        if (page.insert_record(data, size, slot_id)) {
            save_page(page_id, page);
            return RecordId(page_id, slot_id);
        }
    }
    
    // Создаем новую страницу
    uint32_t new_page_id = allocate_new_page();
    Page new_page;
    uint16_t slot_id;
    
    if (!new_page.insert_record(data, size, slot_id)) {
        return RecordId();  // Не удалось вставить (маловероятно для новой страницы)
    }
    
    page_cache_[new_page_id] = new_page;
    save_page(new_page_id, new_page);
    
    return RecordId(new_page_id, slot_id);
}

bool HeapFile::update(const RecordId& rid, const void* data, uint16_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Page* page = get_page(rid.page_id);
    if (!page) {
        return false;
    }
    
    if (page->update_record(rid.slot_id, data, size)) {
        save_page(rid.page_id, *page);
        return true;
    }
    
    return false;
}

bool HeapFile::read_record(const RecordId& rid, void* buffer, uint16_t& size) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const Page* page = get_page(rid.page_id);
    if (!page) {
        return false;
    }
    
    const void* data;
    if (!page->get_record(rid.slot_id, data, size)) {
        return false;
    }
    
    memcpy(buffer, data, size);
    return true;
}

bool HeapFile::remove(const RecordId& rid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Page* page = get_page(rid.page_id);
    if (!page) {
        return false;
    }
    
    if (page->delete_record(rid.slot_id)) {
        save_page(rid.page_id, *page);
        return true;
    }
    
    return false;
}

std::vector<RecordId> HeapFile::scan_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<RecordId> result;
    for (const auto& [page_id, page] : page_cache_) {
        auto page_rids = page.get_all_record_ids(page_id);
        result.insert(result.end(), page_rids.begin(), page_rids.end());
    }
    return result;
}

void HeapFile::flush() {
    if (fd_ >= 0) {
        fsync(fd_);
    }
}

size_t HeapFile::get_record_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [_, page] : page_cache_) {
        count += page.get_slot_count();
    }
    return count;
}

bool HeapFile::load_page(uint32_t page_id, Page& page) const {
    if (fd_ < 0) return false;
    
    char buffer[PAGE_SIZE];
    off_t offset = page_id * PAGE_SIZE;
    
    if (lseek(fd_, offset, SEEK_SET) < 0) return false;
    ssize_t bytes_read = ::read(fd_, buffer, PAGE_SIZE);
    if (bytes_read != PAGE_SIZE) return false;
    
    page.deserialize(buffer);
    return page.verify_checksum();
}

bool HeapFile::save_page(uint32_t page_id, const Page& page) {
    if (fd_ < 0) return false;
    
    char buffer[PAGE_SIZE];
    page.serialize(buffer);
    
    off_t offset = page_id * PAGE_SIZE;
    if (lseek(fd_, offset, SEEK_SET) < 0) return false;
    ssize_t bytes_written = ::write(fd_, buffer, PAGE_SIZE);
    if (bytes_written != PAGE_SIZE) return false;
    
    return true;
}

void HeapFile::load_all_pages() {
    if (fd_ < 0) return;
    
    struct stat st;
    if (fstat(fd_, &st) < 0) return;
    
    size_t page_count = st.st_size / PAGE_SIZE;
    
    for (uint32_t i = 0; i < page_count; i++) {
        Page page;
        if (load_page(i, page)) {
            page_cache_[i] = page;
        }
    }
    
    next_page_id_ = static_cast<uint32_t>(page_count);
}

Page* HeapFile::get_page(uint32_t page_id) {
    auto it = page_cache_.find(page_id);
    if (it != page_cache_.end()) {
        return &it->second;
    }
    
    // Загружаем страницу с диска
    Page page;
    if (load_page(page_id, page)) {
        page_cache_[page_id] = page;
        return &page_cache_[page_id];
    }
    
    return nullptr;
}

const Page* HeapFile::get_page(uint32_t page_id) const {
    auto it = page_cache_.find(page_id);
    if (it != page_cache_.end()) {
        return &it->second;
    }
    return nullptr;
}

uint32_t HeapFile::allocate_new_page() {
    // Опираемся на реальный счётчик страниц на диске, а не на размер кэша:
    // кэш может не содержать всех страниц (например, при ленивой загрузке).
    return next_page_id_++;
}

}