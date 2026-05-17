cmake_minimum_required(VERSION 3.20)

project(pack_audit_decoder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZXing CONFIG REQUIRED)

add_executable(pack-audit-decoder src/main.cpp)
target_link_libraries(pack-audit-decoder PRIVATE ZXing::ZXing)

if(MSVC)
  target_compile_options(pack-audit-decoder PRIVATE /W4 /permissive-)
else()
  target_compile_options(pack-audit-decoder PRIVATE -Wall -Wextra -Wpedantic)
endif()
