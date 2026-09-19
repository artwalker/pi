cmake_minimum_required(VERSION 3.20)
project(pi_native LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

find_package(CURL REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# --- C boundary layer (syscalls; stable ABI) -------------------------------
add_library(pi_c STATIC
  src/c/terminal.c
  src/c/subprocess.c
)
target_include_directories(pi_c PUBLIC include)

# --- C++ core --------------------------------------------------------------
add_executable(pi
  src/cpp/main.cpp
  src/cpp/llm_client.cpp
  src/cpp/agent.cpp
  src/cpp/session_store.cpp
  src/cpp/plugin_loader.cpp
)
target_include_directories(pi PRIVATE include ${SQLITE3_INCLUDE_DIRS})
target_link_libraries(pi PRIVATE pi_c CURL::libcurl nlohmann_json::nlohmann_json ${SQLITE3_LIBRARIES} ${CMAKE_DL_LIBS})

# --- Example runtime plugin (C-ABI shared object) --------------------------
add_library(wordcount_plugin SHARED plugins/wordcount/wordcount_plugin.c)
target_include_directories(wordcount_plugin PRIVATE include)
set_target_properties(wordcount_plugin PROPERTIES PREFIX "" OUTPUT_NAME "wordcount")
