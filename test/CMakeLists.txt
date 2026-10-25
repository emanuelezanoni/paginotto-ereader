cmake_minimum_required(VERSION 3.13)
project(paginotto_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Source modules under test
#
# Pure modules — must compile cleanly without Arduino.h.
# KV-store-backed modules — also host-compatible (their firmware-only code
# is inside `#ifdef ARDUINO` blocks).
# ---------------------------------------------------------------------------
set(FW_SRC ${CMAKE_SOURCE_DIR}/../Paginotto/src)
set(SUT_SRC
  ${FW_SRC}/pure/paths.cpp
  ${FW_SRC}/pure/text_util.cpp
  ${FW_SRC}/pure/hashing.cpp
  ${FW_SRC}/pure/paginator.cpp
  ${FW_SRC}/pure/library_nav.cpp
  ${FW_SRC}/pure/library_catalog.cpp
  ${FW_SRC}/pure/page_cache_format.cpp
  ${FW_SRC}/pure/reader_resume.cpp
  ${FW_SRC}/pure/reader_scan.cpp
  ${FW_SRC}/pure/reader_navigation.cpp
  ${FW_SRC}/pure/reader_integration.cpp
  ${FW_SRC}/pure/hold_gesture.cpp
  ${FW_SRC}/pure/button_policy.cpp
  ${FW_SRC}/pure/bionic_text.cpp
  ${FW_SRC}/pure/device_policy.cpp
  ${FW_SRC}/pure/ota_policy.cpp
  ${FW_SRC}/pure/screensaver_policy.cpp
  ${FW_SRC}/pure/wifi_credentials.cpp
  ${FW_SRC}/storage/book_metadata.cpp
)

set(TEST_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_paths.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_text_util.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_hashing.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_paginator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_library_nav.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_library_catalog.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_book_metadata.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_page_cache_format.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_reader_resume.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_reader_scan.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_reader_navigation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_reader_integration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_hold_gesture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_button_policy.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_bionic_text.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_device_policy.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_ota_policy.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_screensaver_policy.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_wifi_credentials.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_stream.cpp
)

add_executable(host_tests ${SUT_SRC} ${TEST_SRC})
target_include_directories(host_tests PRIVATE
  ${FW_SRC}
  ${CMAKE_SOURCE_DIR}/../Paginotto
  ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
  # /W4 with two suppressions:
  #   C4310 — "cast truncates constant value" — deliberate when building
  #           raw UTF-8 byte arrays in tests like `(char)0xC3`.
  target_compile_options(host_tests PRIVATE /W4 /permissive- /wd4310)
  # Silence "use strcpy_s / sprintf_s" CRT-deprecation warnings — those
  # functions are deliberate, and we're targeting C++17 portably.
  target_compile_definitions(host_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(host_tests PRIVATE -Wall -Wextra -Wpedantic
                                            -Wno-unused-function)
endif()

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
