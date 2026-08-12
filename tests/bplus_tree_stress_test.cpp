#include <iostream>
#include <set>
#include <cassert>
#include <random>
#include <algorithm>
#include "bplus_tree.hpp"
#include "value.hpp"
#include "record_id.hpp"

using namespace sysdb;

void test_full_lifecycle() {
    std::cout << "=== test_full_lifecycle (int keys, insert+delete everything) ===" << std::endl;
    remove("t1.idx");
    BPlusTree tree("t1", Value::Type::INT);
    assert(tree.initialize());

    const int N = 2000;
    for (int i = 0; i < N; i++) {
        tree.insert(Value::make_int(i), RecordId(1, static_cast<uint16_t>(i % 60000)));
    }
    for (int i = 0; i < N; i++) {
        auto r = tree.find(Value::make_int(i));
        assert(r.size() == 1);
    }
    auto range = tree.range_find(Value::make_int(500), Value::make_int(510));
    assert(range.size() == 10);

    // Удаляем в случайном порядке, чтобы протестировать все виды merge/borrow
    std::vector<int> order(N);
    for (int i = 0; i < N; i++) order[i] = i;
    std::mt19937 rng(42);
    std::shuffle(order.begin(), order.end(), rng);

    std::set<int> remaining(order.begin(), order.end());
    for (int idx : order) {
        tree.remove(Value::make_int(idx), RecordId(1, static_cast<uint16_t>(idx % 60000)));
        remaining.erase(idx);
        if (idx % 137 == 0) {
            // периодически проверяем целостность оставшихся ключей
            for (int check : remaining) {
                auto r = tree.find(Value::make_int(check));
                if (r.size() != 1) {
                    std::cout << "FAIL: key " << check << " missing after deleting " << idx << std::endl;
                    assert(false);
                }
            }
            auto deleted_check = tree.find(Value::make_int(idx));
            assert(deleted_check.empty());
        }
    }

    for (int i = 0; i < N; i++) {
        auto r = tree.find(Value::make_int(i));
        assert(r.empty());
    }
    std::cout << "OK: full lifecycle with " << N << " keys passed" << std::endl;
}

void test_string_keys() {
    std::cout << "=== test_string_keys (with truncation) ===" << std::endl;
    remove("t2.idx");
    BPlusTree tree("t2", Value::Type::STRING);
    assert(tree.initialize());

    tree.insert(Value::make_string("apple"), RecordId(1, 1));
    tree.insert(Value::make_string("banana"), RecordId(1, 2));
    tree.insert(Value::make_string("cherry"), RecordId(1, 3));

    std::string long_str(300, 'x'); // длиннее MAX_KEY_LEN=255, будет обрезана при сравнении ключей индекса
    tree.insert(Value::make_string(long_str), RecordId(1, 4));

    auto r1 = tree.find(Value::make_string("banana"));
    assert(r1.size() == 1 && r1[0].slot_id == 2);

    std::string truncated = long_str.substr(0, 255);
    auto r2 = tree.find(Value::make_string(truncated));
    assert(r2.size() == 1 && r2[0].slot_id == 4);

    auto range = tree.range_find(Value::make_string("apple"), Value::make_string("cherry"));
    assert(range.size() == 2); // apple, banana (cherry не входит, т.к. полуоткрытый диапазон)

    std::cout << "OK: string keys work correctly" << std::endl;
}

int main() {
    test_full_lifecycle();
    test_string_keys();
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
