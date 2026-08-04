#ifndef STORAGE_STUB_HPP
#define STORAGE_STUB_HPP

#include <iostream>
#include <string>

namespace sysdb {

    class StorageStub {
    public:
        bool databaseExists(const std::string& name) {
            // Для теста будем считать, что база "test_db" уже существует
            return name == "test_db";
        }

        void createDatabase(const std::string& name) {
            std::cout << "[STORAGE] Database '" << name << "' created successfully." << std::endl;
        }

        void dropDatabase(const std::string& name) {
            std::cout << "[STORAGE] Database '" << name << "' dropped successfully." << std::endl;
        }

        void useDatabase(const std::string& name) {
            std::cout << "[STORAGE] Switched to database '" << name << "'." << std::endl;
        }
    };

} // namespace sysdb

#endif // STORAGE_STUB_HPP