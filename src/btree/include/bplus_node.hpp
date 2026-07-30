#pragma once

#include <cstdint>
#include <cstring>
#include "index_key.hpp"
#include "record_id.hpp"

namespace sysdb {

constexpr uint32_t INDEX_PAGE_SIZE = 4096;
constexpr uint32_t INVALID_PAGE = 0xFFFFFFFFu;

// Запись листового узла: ключ + ссылка на реальные данные записи.
// Данные не дублируются - индекс хранит только RecordId.
struct LeafEntry {
    IndexKey key;
    RecordId rid;
};

struct BTreeNodeHeader {
    uint8_t is_leaf = 1;
    uint16_t num_keys = 0;
    uint32_t next_leaf = INVALID_PAGE;  // используется только листами (связный список для range-сканов)
};

constexpr size_t NODE_HEADER_SIZE = sizeof(BTreeNodeHeader);

constexpr size_t LEAF_ENTRY_SIZE = sizeof(LeafEntry);
constexpr size_t MAX_LEAF_KEYS = (INDEX_PAGE_SIZE - NODE_HEADER_SIZE) / LEAF_ENTRY_SIZE;

// Внутренний узел: num_keys ключей и (num_keys + 1) указателей на детей.
constexpr size_t CHILD_PTR_SIZE = sizeof(uint32_t);
constexpr size_t INTERNAL_ENTRY_SIZE = sizeof(IndexKey) + CHILD_PTR_SIZE;
constexpr size_t MAX_INTERNAL_KEYS =
    (INDEX_PAGE_SIZE - NODE_HEADER_SIZE - CHILD_PTR_SIZE) / INTERNAL_ENTRY_SIZE;

static_assert(MAX_LEAF_KEYS >= 3, "Leaf must fit at least a few keys per page");
static_assert(MAX_INTERNAL_KEYS >= 3, "Internal node must fit at least a few keys per page");

// Минимальная заполненность (для нелистовых/нелистовых узлов, кроме корня).
constexpr size_t MIN_LEAF_KEYS = MAX_LEAF_KEYS / 2;
constexpr size_t MIN_INTERNAL_KEYS = MAX_INTERNAL_KEYS / 2;

/**
BTreeNode - узел B+-дерева в памяти. На диске хранится ровно одна
физическая страница INDEX_PAGE_SIZE; сериализация/десериализация
пишет/читает только актуальную часть (в зависимости от is_leaf и
фактического num_keys), а не весь объект целиком.
*/
struct BTreeNode {
    BTreeNodeHeader header;

    // Активно только одно из двух представлений (по header.is_leaf).
    LeafEntry leaf_entries[MAX_LEAF_KEYS];

    IndexKey internal_keys[MAX_INTERNAL_KEYS];
    uint32_t children[MAX_INTERNAL_KEYS + 1];

    BTreeNode() {
        header.is_leaf = 1;
        header.num_keys = 0;
        header.next_leaf = INVALID_PAGE;
        for (auto& c : children) c = INVALID_PAGE;
    }

    void serialize(char* buffer) const {
        memset(buffer, 0, INDEX_PAGE_SIZE);
        size_t off = 0;
        memcpy(buffer + off, &header, sizeof(header));
        off += sizeof(header);

        if (header.is_leaf) {
            memcpy(buffer + off, leaf_entries, header.num_keys * sizeof(LeafEntry));
        } else {
            memcpy(buffer + off, internal_keys, header.num_keys * sizeof(IndexKey));
            off += header.num_keys * sizeof(IndexKey);
            memcpy(buffer + off, children, (header.num_keys + 1) * sizeof(uint32_t));
        }
    }

    void deserialize(const char* buffer) {
        size_t off = 0;
        memcpy(&header, buffer + off, sizeof(header));
        off += sizeof(header);

        for (auto& c : children) c = INVALID_PAGE;

        if (header.is_leaf) {
            memcpy(leaf_entries, buffer + off, header.num_keys * sizeof(LeafEntry));
        } else {
            memcpy(internal_keys, buffer + off, header.num_keys * sizeof(IndexKey));
            off += header.num_keys * sizeof(IndexKey);
            memcpy(children, buffer + off, (header.num_keys + 1) * sizeof(uint32_t));
        }
    }
};

}
