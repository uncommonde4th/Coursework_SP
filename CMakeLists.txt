cmake_minimum_required(VERSION 3.14)
project(SysDB VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции компиляции
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -g")

# Находим все .cpp файлы в src/
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*/*.cpp"
    "src/*/*/*.cpp"
)

# Добавляем main.cpp
list(APPEND SOURCES main.cpp)

# Создаем исполняемый файл
add_executable(sysdb
        main.cpp
        src/cli/cli.cpp
        src/parser/tokenizer.cpp
        src/parser/parser.cpp
)

# Добавляем include директории
target_include_directories(sysdb PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/record/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/btree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/types/include
)

# Ссылка на pthread
target_link_libraries(sysdb pthread)