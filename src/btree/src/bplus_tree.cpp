#include "bplus_tree.hpp"
#include "core/utils/platform.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace sysdb {

BPlusTree::BPlusTree(const std::string& index_name, Value::Type key_type)
    : index_name_(index_name), key_type_(key_type) {}

BPlusTree::~BPlusTree() {
    if (fd_ >= 0) {
        flush();
        ::close(fd_);
        fd_ = -1;
    }
}

bool BPlusTree::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string filename = index_name_ + ".idx";
    fd_ = ::open(filename.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd_ < 0) {
        return false;
    }

    struct stat st;
    if (fstat(fd_, &st) < 0) {
        return false;
    }

    if (st.st_size == 0) {
        // Новый файл индекса
        meta_.root_page = 1;
        meta_.next_page_id = 2;
        meta_.key_type = static_cast<uint8_t>(key_type_);

        BTreeNode root;
        root.header.is_leaf = 1;
        root.header.num_keys = 0;
        root.header.next_leaf = INVALID_PAGE;
        write_node(meta_.root_page, root);
        save_meta();
    } else {
        load_meta();
        key_type_ = static_cast<Value::Type>(meta_.key_type);
    }

    return true;
}

void BPlusTree::flush() {
    if (fd_ >= 0) {
        platform_fsync(fd_);
    }
}

// Низкоуровневый ввод-вывод

bool BPlusTree::read_page_raw(uint32_t page_id, char* buffer) const {
    if (fd_ < 0) return false;
    off_t offset = static_cast<off_t>(page_id) * INDEX_PAGE_SIZE;
    if (lseek(fd_, offset, SEEK_SET) < 0) return false;
    ssize_t n = ::read(fd_, buffer, INDEX_PAGE_SIZE);
    return n == static_cast<ssize_t>(INDEX_PAGE_SIZE);
}

bool BPlusTree::write_page_raw(uint32_t page_id, const char* buffer) {
    if (fd_ < 0) return false;
    off_t offset = static_cast<off_t>(page_id) * INDEX_PAGE_SIZE;
    if (lseek(fd_, offset, SEEK_SET) < 0) return false;
    ssize_t n = ::write(fd_, buffer, INDEX_PAGE_SIZE);
    return n == static_cast<ssize_t>(INDEX_PAGE_SIZE);
}

void BPlusTree::load_meta() {
    char buffer[INDEX_PAGE_SIZE];
    if (!read_page_raw(0, buffer)) {
        throw std::runtime_error("BPlusTree: failed to read meta page");
    }
    memcpy(&meta_, buffer, sizeof(TreeMeta));
}

void BPlusTree::save_meta() {
    char buffer[INDEX_PAGE_SIZE];
    memset(buffer, 0, INDEX_PAGE_SIZE);
    memcpy(buffer, &meta_, sizeof(TreeMeta));
    write_page_raw(0, buffer);
}

uint32_t BPlusTree::allocate_page() {
    uint32_t id = meta_.next_page_id++;
    save_meta();
    return id;
}

BTreeNode BPlusTree::read_node(uint32_t page_id) const {
    char buffer[INDEX_PAGE_SIZE];
    BTreeNode node;
    if (!read_page_raw(page_id, buffer)) {
        throw std::runtime_error("BPlusTree: failed to read node page " + std::to_string(page_id));
    }
    node.deserialize(buffer);
    return node;
}

void BPlusTree::write_node(uint32_t page_id, const BTreeNode& node) {
    char buffer[INDEX_PAGE_SIZE];
    node.serialize(buffer);
    if (!write_page_raw(page_id, buffer)) {
        throw std::runtime_error("BPlusTree: failed to write node page " + std::to_string(page_id));
    }
}

// Поиск

int BPlusTree::find_child_index(const BTreeNode& node, const IndexKey& key) const {
    for (uint16_t i = 0; i < node.header.num_keys; i++) {
        if (key < node.internal_keys[i]) {
            return i;
        }
    }
    return node.header.num_keys;
}

uint32_t BPlusTree::find_leaf_page(const IndexKey& key) const {
    uint32_t page = meta_.root_page;
    BTreeNode node = read_node(page);
    while (!node.header.is_leaf) {
        int idx = find_child_index(node, key);
        page = node.children[idx];
        node = read_node(page);
    }
    return page;
}

std::vector<RecordId> BPlusTree::find(const Value& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (meta_.root_page == INVALID_PAGE) return {};

    IndexKey k = IndexKey::from_value(key);
    uint32_t leaf_page = find_leaf_page(k);
    BTreeNode leaf = read_node(leaf_page);

    std::vector<RecordId> result;
    for (uint16_t i = 0; i < leaf.header.num_keys; i++) {
        if (leaf.leaf_entries[i].key == k) {
            result.push_back(leaf.leaf_entries[i].rid);
        }
    }
    return result;
}

std::vector<RecordId> BPlusTree::range_find(const Value& start, const Value& end) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (meta_.root_page == INVALID_PAGE) return {};

    IndexKey k_start = IndexKey::from_value(start);
    IndexKey k_end = IndexKey::from_value(end);

    std::vector<RecordId> result;
    uint32_t page = find_leaf_page(k_start);

    while (page != INVALID_PAGE) {
        BTreeNode leaf = read_node(page);
        bool past_end = false;
        for (uint16_t i = 0; i < leaf.header.num_keys; i++) {
            const IndexKey& k = leaf.leaf_entries[i].key;
            if (k < k_start) continue;
            if (!(k < k_end)) { past_end = true; break; }
            result.push_back(leaf.leaf_entries[i].rid);
        }
        if (past_end) break;
        page = leaf.header.next_leaf;
    }
    return result;
}

// Вставка

void BPlusTree::insert(const Value& key, const RecordId& rid) {
    std::lock_guard<std::mutex> lock(mutex_);

    IndexKey k = IndexKey::from_value(key);
    IndexKey promoted;
    uint32_t new_right = INVALID_PAGE;

    bool split = insert_into(meta_.root_page, k, rid, promoted, new_right);

    if (split) {
        uint32_t new_root_page = allocate_page();
        BTreeNode new_root;
        new_root.header.is_leaf = 0;
        new_root.header.num_keys = 1;
        new_root.internal_keys[0] = promoted;
        new_root.children[0] = meta_.root_page;
        new_root.children[1] = new_right;
        write_node(new_root_page, new_root);

        meta_.root_page = new_root_page;
        save_meta();
    }
}

bool BPlusTree::insert_into(uint32_t node_page, const IndexKey& key, const RecordId& rid,
                             IndexKey& promoted_key, uint32_t& new_right_page) {
    BTreeNode node = read_node(node_page);

    if (node.header.is_leaf) {
        int pos = 0;
        while (pos < node.header.num_keys && node.leaf_entries[pos].key <= key) {
            pos++;
        }

        if (node.header.num_keys < static_cast<int>(MAX_LEAF_KEYS)) {
            for (int i = node.header.num_keys; i > pos; i--) {
                node.leaf_entries[i] = node.leaf_entries[i - 1];
            }
            node.leaf_entries[pos].key = key;
            node.leaf_entries[pos].rid = rid;
            node.header.num_keys++;
            write_node(node_page, node);
            return false;
        }

        std::vector<LeafEntry> tmp(node.leaf_entries, node.leaf_entries + node.header.num_keys);
        LeafEntry new_entry{key, rid};
        tmp.insert(tmp.begin() + pos, new_entry);

        size_t total = tmp.size();
        size_t left_count = total / 2;

        BTreeNode left_node;
        left_node.header.is_leaf = 1;
        left_node.header.num_keys = static_cast<uint16_t>(left_count);
        for (size_t i = 0; i < left_count; i++) left_node.leaf_entries[i] = tmp[i];

        uint32_t right_page = allocate_page();
        BTreeNode right_node;
        right_node.header.is_leaf = 1;
        right_node.header.num_keys = static_cast<uint16_t>(total - left_count);
        for (size_t i = left_count; i < total; i++) {
            right_node.leaf_entries[i - left_count] = tmp[i];
        }

        right_node.header.next_leaf = node.header.next_leaf;
        left_node.header.next_leaf = right_page;

        write_node(node_page, left_node);
        write_node(right_page, right_node);

        promoted_key = right_node.leaf_entries[0].key;
        new_right_page = right_page;
        return true;
    }

    int idx = find_child_index(node, key);
    IndexKey child_promoted;
    uint32_t child_new_right = INVALID_PAGE;

    bool child_split = insert_into(node.children[idx], key, rid, child_promoted, child_new_right);
    if (!child_split) {
        return false;
    }

    if (node.header.num_keys < static_cast<int>(MAX_INTERNAL_KEYS)) {
        for (int i = node.header.num_keys; i > idx; i--) {
            node.internal_keys[i] = node.internal_keys[i - 1];
        }
        for (int i = node.header.num_keys + 1; i > idx + 1; i--) {
            node.children[i] = node.children[i - 1];
        }
        node.internal_keys[idx] = child_promoted;
        node.children[idx + 1] = child_new_right;
        node.header.num_keys++;
        write_node(node_page, node);
        return false;
    }

    std::vector<IndexKey> tmp_keys(node.internal_keys, node.internal_keys + node.header.num_keys);
    std::vector<uint32_t> tmp_children(node.children, node.children + node.header.num_keys + 1);

    tmp_keys.insert(tmp_keys.begin() + idx, child_promoted);
    tmp_children.insert(tmp_children.begin() + idx + 1, child_new_right);

    size_t total_keys = tmp_keys.size();
    size_t mid = total_keys / 2;

    BTreeNode left_node;
    left_node.header.is_leaf = 0;
    left_node.header.num_keys = static_cast<uint16_t>(mid);
    for (size_t i = 0; i < mid; i++) left_node.internal_keys[i] = tmp_keys[i];
    for (size_t i = 0; i <= mid; i++) left_node.children[i] = tmp_children[i];

    uint32_t right_page = allocate_page();
    BTreeNode right_node;
    right_node.header.is_leaf = 0;
    right_node.header.num_keys = static_cast<uint16_t>(total_keys - mid - 1);
    for (size_t i = mid + 1; i < total_keys; i++) {
        right_node.internal_keys[i - mid - 1] = tmp_keys[i];
    }
    for (size_t i = mid + 1; i < tmp_children.size(); i++) {
        right_node.children[i - mid - 1] = tmp_children[i];
    }

    write_node(node_page, left_node);
    write_node(right_page, right_node);

    promoted_key = tmp_keys[mid];
    new_right_page = right_page;
    return true;
}

// Удаление

void BPlusTree::remove(const Value& key, const RecordId& rid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (meta_.root_page == INVALID_PAGE) return;

    IndexKey k = IndexKey::from_value(key);
    bool underflow = false;
    bool found = remove_from(meta_.root_page, k, rid, underflow);
    if (!found) return;

    BTreeNode root = read_node(meta_.root_page);
    if (!root.header.is_leaf && root.header.num_keys == 0) {
        meta_.root_page = root.children[0];
        save_meta();
    }
}

bool BPlusTree::remove_from(uint32_t node_page, const IndexKey& key, const RecordId& rid,
                             bool& underflow) {
    BTreeNode node = read_node(node_page);
    bool is_root = (node_page == meta_.root_page);

    if (node.header.is_leaf) {
        int found_idx = -1;
        for (uint16_t i = 0; i < node.header.num_keys; i++) {
            if (node.leaf_entries[i].key == key && node.leaf_entries[i].rid == rid) {
                found_idx = i;
                break;
            }
        }
        if (found_idx < 0) {
            underflow = false;
            return false;
        }

        for (int i = found_idx; i < node.header.num_keys - 1; i++) {
            node.leaf_entries[i] = node.leaf_entries[i + 1];
        }
        node.header.num_keys--;
        write_node(node_page, node);

        underflow = (!is_root && node.header.num_keys < static_cast<int>(MIN_LEAF_KEYS));
        return true;
    }

    int idx = find_child_index(node, key);
    uint32_t child_page = node.children[idx];

    bool child_underflow = false;
    bool found = remove_from(child_page, key, rid, child_underflow);
    if (!found) {
        underflow = false;
        return false;
    }

    if (child_underflow) {
        bool child_is_leaf = read_node(child_page).header.is_leaf;
        rebalance_child(node, node_page, idx, child_is_leaf);
    }

    write_node(node_page, node);
    underflow = (!is_root && node.header.num_keys < static_cast<int>(MIN_INTERNAL_KEYS));
    return true;
}

void BPlusTree::rebalance_child(BTreeNode& parent, uint32_t parent_page,
                                 int child_index, bool child_is_leaf) {
    (void)parent_page;
    uint32_t child_page = parent.children[child_index];
    BTreeNode child = read_node(child_page);

    bool has_left = child_index > 0;
    bool has_right = child_index < parent.header.num_keys;

    if (child_is_leaf) {
        if (has_left) {
            uint32_t left_page = parent.children[child_index - 1];
            BTreeNode left = read_node(left_page);
            if (left.header.num_keys > static_cast<int>(MIN_LEAF_KEYS)) {
                LeafEntry borrowed = left.leaf_entries[left.header.num_keys - 1];
                left.header.num_keys--;

                for (int i = child.header.num_keys; i > 0; i--) {
                    child.leaf_entries[i] = child.leaf_entries[i - 1];
                }
                child.leaf_entries[0] = borrowed;
                child.header.num_keys++;

                parent.internal_keys[child_index - 1] = child.leaf_entries[0].key;

                write_node(left_page, left);
                write_node(child_page, child);
                return;
            }
        }
        if (has_right) {
            uint32_t right_page = parent.children[child_index + 1];
            BTreeNode right = read_node(right_page);
            if (right.header.num_keys > static_cast<int>(MIN_LEAF_KEYS)) {
                LeafEntry borrowed = right.leaf_entries[0];
                for (int i = 0; i < right.header.num_keys - 1; i++) {
                    right.leaf_entries[i] = right.leaf_entries[i + 1];
                }
                right.header.num_keys--;

                child.leaf_entries[child.header.num_keys] = borrowed;
                child.header.num_keys++;

                parent.internal_keys[child_index] = right.leaf_entries[0].key;

                write_node(right_page, right);
                write_node(child_page, child);
                return;
            }
        }

        if (has_left) {
            uint32_t left_page = parent.children[child_index - 1];
            BTreeNode left = read_node(left_page);

            for (int i = 0; i < child.header.num_keys; i++) {
                left.leaf_entries[left.header.num_keys + i] = child.leaf_entries[i];
            }
            left.header.num_keys += child.header.num_keys;
            left.header.next_leaf = child.header.next_leaf;

            write_node(left_page, left);

            for (int i = child_index - 1; i < parent.header.num_keys - 1; i++) {
                parent.internal_keys[i] = parent.internal_keys[i + 1];
            }
            for (int i = child_index; i < parent.header.num_keys; i++) {
                parent.children[i] = parent.children[i + 1];
            }
            parent.header.num_keys--;
        } else {
            uint32_t right_page = parent.children[child_index + 1];
            BTreeNode right = read_node(right_page);

            for (int i = 0; i < right.header.num_keys; i++) {
                child.leaf_entries[child.header.num_keys + i] = right.leaf_entries[i];
            }
            child.header.num_keys += right.header.num_keys;
            child.header.next_leaf = right.header.next_leaf;

            write_node(child_page, child);

            for (int i = child_index; i < parent.header.num_keys - 1; i++) {
                parent.internal_keys[i] = parent.internal_keys[i + 1];
            }
            for (int i = child_index + 1; i < parent.header.num_keys; i++) {
                parent.children[i] = parent.children[i + 1];
            }
            parent.header.num_keys--;
        }
        return;
    }

    // Внутренний узел-ребёнок
    if (has_left) {
        uint32_t left_page = parent.children[child_index - 1];
        BTreeNode left = read_node(left_page);
        if (left.header.num_keys > static_cast<int>(MIN_INTERNAL_KEYS)) {
            for (int i = child.header.num_keys; i > 0; i--) {
                child.internal_keys[i] = child.internal_keys[i - 1];
            }
            for (int i = child.header.num_keys + 1; i > 0; i--) {
                child.children[i] = child.children[i - 1];
            }
            child.internal_keys[0] = parent.internal_keys[child_index - 1];
            child.children[0] = left.children[left.header.num_keys];
            child.header.num_keys++;

            parent.internal_keys[child_index - 1] = left.internal_keys[left.header.num_keys - 1];
            left.header.num_keys--;

            write_node(left_page, left);
            write_node(child_page, child);
            return;
        }
    }
    if (has_right) {
        uint32_t right_page = parent.children[child_index + 1];
        BTreeNode right = read_node(right_page);
        if (right.header.num_keys > static_cast<int>(MIN_INTERNAL_KEYS)) {
            child.internal_keys[child.header.num_keys] = parent.internal_keys[child_index];
            child.children[child.header.num_keys + 1] = right.children[0];
            child.header.num_keys++;

            parent.internal_keys[child_index] = right.internal_keys[0];

            for (int i = 0; i < right.header.num_keys - 1; i++) {
                right.internal_keys[i] = right.internal_keys[i + 1];
            }
            for (int i = 0; i < right.header.num_keys; i++) {
                right.children[i] = right.children[i + 1];
            }
            right.header.num_keys--;

            write_node(right_page, right);
            write_node(child_page, child);
            return;
        }
    }

    if (has_left) {
        uint32_t left_page = parent.children[child_index - 1];
        BTreeNode left = read_node(left_page);

        left.internal_keys[left.header.num_keys] = parent.internal_keys[child_index - 1];
        for (int i = 0; i < child.header.num_keys; i++) {
            left.internal_keys[left.header.num_keys + 1 + i] = child.internal_keys[i];
        }
        for (int i = 0; i <= child.header.num_keys; i++) {
            left.children[left.header.num_keys + 1 + i] = child.children[i];
        }
        left.header.num_keys += child.header.num_keys + 1;

        write_node(left_page, left);

        for (int i = child_index - 1; i < parent.header.num_keys - 1; i++) {
            parent.internal_keys[i] = parent.internal_keys[i + 1];
        }
        for (int i = child_index; i < parent.header.num_keys; i++) {
            parent.children[i] = parent.children[i + 1];
        }
        parent.header.num_keys--;
    } else {
        uint32_t right_page = parent.children[child_index + 1];
        BTreeNode right = read_node(right_page);

        child.internal_keys[child.header.num_keys] = parent.internal_keys[child_index];
        for (int i = 0; i < right.header.num_keys; i++) {
            child.internal_keys[child.header.num_keys + 1 + i] = right.internal_keys[i];
        }
        for (int i = 0; i <= right.header.num_keys; i++) {
            child.children[child.header.num_keys + 1 + i] = right.children[i];
        }
        child.header.num_keys += right.header.num_keys + 1;

        write_node(child_page, child);

        for (int i = child_index; i < parent.header.num_keys - 1; i++) {
            parent.internal_keys[i] = parent.internal_keys[i + 1];
        }
        for (int i = child_index + 1; i < parent.header.num_keys; i++) {
            parent.children[i] = parent.children[i + 1];
        }
        parent.header.num_keys--;
    }
}

}
