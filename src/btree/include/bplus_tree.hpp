#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "record_id.hpp"
#include "value.hpp"
#include "index_key.hpp"
#include "bplus_node.hpp"

namespace sysdb {

/*
BPlusTree - персистентный индекс поверх страничного файла.

Формат файла индекса "<index_name>.idx":
   страница 0 - метаданные дерева (корень, счётчик страниц, тип ключа)
   страницы 1..N-1 - узлы дерева (BTreeNode), по одной физической
                     странице INDEX_PAGE_SIZE на узел

 Листовые узлы связаны в односвязный список (next_leaf) для
 последовательного сканирования диапазонов (range_find).
 Индекс хранит только (key -> RecordId), сами данные не дублируются.
*/
class BPlusTree {
public:
    BPlusTree(const std::string& index_name, Value::Type key_type);
    ~BPlusTree();

    bool initialize();

    void insert(const Value& key, const RecordId& rid);
    void remove(const Value& key, const RecordId& rid);
    std::vector<RecordId> find(const Value& key);
    std::vector<RecordId> range_find(const Value& start, const Value& end); // [start, end)

    void flush();

private:
    std::string index_name_;
    Value::Type key_type_;
    mutable std::mutex mutex_;

    int fd_ = -1;

    struct TreeMeta {
        uint32_t root_page = INVALID_PAGE;
        uint32_t next_page_id = 1; // страница 0 занята метаданными
        uint8_t key_type = 0;
    } meta_;

    // низкоуровневый ввод-вывод страниц
    bool read_page_raw(uint32_t page_id, char* buffer) const;
    bool write_page_raw(uint32_t page_id, const char* buffer);

    void load_meta();
    void save_meta();

    uint32_t allocate_page();

    BTreeNode read_node(uint32_t page_id) const;
    void write_node(uint32_t page_id, const BTreeNode& node);

    // вставка
    // Возвращает true, если узел разделился. тогда promoted_key/new_right_page
    // должны быть добавлены родителем.
    bool insert_into(uint32_t node_page, const IndexKey& key, const RecordId& rid,
                      IndexKey& promoted_key, uint32_t& new_right_page);

    void split_leaf(uint32_t node_page, BTreeNode& node,
                     IndexKey& promoted_key, uint32_t& new_right_page);
    void split_internal(uint32_t node_page, BTreeNode& node,
                         IndexKey& promoted_key, uint32_t& new_right_page);

    // удаление
    // Удаляет (key, rid) из поддерева node_page. Возвращает true, если после
    // удаления узел стал меньше минимальной заполненности (родитель должен
    // разобраться с перераспределением/слиянием).
    bool remove_from(uint32_t node_page, const IndexKey& key, const RecordId& rid,
                      bool& underflow);

    void rebalance_child(BTreeNode& parent, uint32_t parent_page,
                          int child_index, bool child_is_leaf);

    // поиск
    uint32_t find_leaf_page(const IndexKey& key) const;
    int find_child_index(const BTreeNode& node, const IndexKey& key) const;
};

}
