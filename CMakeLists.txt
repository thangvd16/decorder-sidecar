cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)

project(pack_audit_zbar_decoder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pack-audit-decoder src/main.cpp)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(ZBAR QUIET IMPORTED_TARGET zbar)
endif()

if(TARGET PkgConfig::ZBAR)
  target_link_libraries(pack-audit-decoder PRIVATE PkgConfig::ZBAR)
else()
  find_path(ZBAR_INCLUDE_DIR NAMES zbar.h REQUIRED)
  find_library(ZBAR_LIBRARY NAMES zbar libzbar REQUIRED)
  find_library(ICONV_LIBRARY NAMES iconv libiconv charset libcharset)

  target_include_directories(pack-audit-decoder PRIVATE "${ZBAR_INCLUDE_DIR}")
  target_link_libraries(pack-audit-decoder PRIVATE "${ZBAR_LIBRARY}")

  if(ICONV_LIBRARY)
    target_link_libraries(pack-audit-decoder PRIVATE "${ICONV_LIBRARY}")
  endif()
endif()

if(MSVC)
  set_property(TARGET pack-audit-decoder PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(pack-audit-decoder PRIVATE /W4 /permissive-)
else()
  target_compile_options(pack-audit-decoder PRIVATE -Wall -Wextra -Wpedantic)
endif()
