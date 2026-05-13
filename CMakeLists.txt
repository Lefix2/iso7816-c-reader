cmake_minimum_required(VERSION 3.16)

project(iso7816
    VERSION 1.0.0
    LANGUAGES C
)

# ────────────────────────────────────────────────────────────────────────────────
# Global configuration
# ────────────────────────────────────────────────────────────────────────────────

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CTest)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

set(SC_MAX_SLOTS 1 CACHE STRING "Maximum number of registered smartcard slots")

# ────────────────────────────────────────────────────────────────────────────────
# Sanitizers (ASAN)
# ────────────────────────────────────────────────────────────────────────────────

add_library(iso7816_sanitizers INTERFACE)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")

    target_compile_options(iso7816_sanitizers INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer
    )

    target_link_options(iso7816_sanitizers INTERFACE
        -fsanitize=address
    )
endif()

# ────────────────────────────────────────────────────────────────────────────────
# Coverage
# ────────────────────────────────────────────────────────────────────────────────

add_library(iso7816_coverage INTERFACE)

if(ENABLE_COVERAGE)
    message(STATUS "Coverage enabled")

    target_compile_options(iso7816_coverage INTERFACE
        --coverage
        -O0
        -g
    )

    target_link_options(iso7816_coverage INTERFACE
        --coverage
    )
endif()

# ────────────────────────────────────────────────────────────────────────────────
# Sources
# ────────────────────────────────────────────────────────────────────────────────

set(ISO7816_SOURCES
    src/smartcard.c
    src/sc_debug.c
    src/sc_defs.c
    src/maths/EDC.c
    src/protocols/protocol_atr.c
    src/protocols/protocol_pps.c
    src/protocols/protocol_TPDU_T0.c
    src/protocols/protocol_APDU_T0.c
    src/protocols/protocol_TPDU_T1.c
    src/protocols/protocol_APDU_T1.c
)

# ────────────────────────────────────────────────────────────────────────────────
# Warnings
# ────────────────────────────────────────────────────────────────────────────────

add_library(iso7816_warnings INTERFACE)

target_compile_options(iso7816_warnings INTERFACE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wstrict-prototypes
    >
)

# ────────────────────────────────────────────────────────────────────────────────
# Object library
# ────────────────────────────────────────────────────────────────────────────────

add_library(iso7816_objects OBJECT ${ISO7816_SOURCES})

set_target_properties(iso7816_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(iso7816_objects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

target_link_libraries(iso7816_objects
    PRIVATE
        iso7816_warnings
        iso7816_sanitizers
        iso7816_coverage
)

target_compile_definitions(iso7816_objects
    PUBLIC SC_MAX_SLOTS=${SC_MAX_SLOTS}
)

# ────────────────────────────────────────────────────────────────────────────────
# Static library
# ────────────────────────────────────────────────────────────────────────────────

add_library(iso7816_static STATIC
    $<TARGET_OBJECTS:iso7816_objects>
)

add_library(iso7816::static ALIAS iso7816_static)

target_link_libraries(iso7816_static
    PRIVATE
        iso7816_sanitizers
        iso7816_coverage
)

set_target_properties(iso7816_static PROPERTIES
    OUTPUT_NAME iso7816
)

# ────────────────────────────────────────────────────────────────────────────────
# Shared library
# ────────────────────────────────────────────────────────────────────────────────

add_library(iso7816_shared SHARED
    $<TARGET_OBJECTS:iso7816_objects>
)

add_library(iso7816::shared ALIAS iso7816_shared)

target_link_libraries(iso7816_shared
    PRIVATE
        iso7816_sanitizers
        iso7816_coverage
)

set_target_properties(iso7816_shared PROPERTIES
    OUTPUT_NAME iso7816
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ────────────────────────────────────────────────────────────────────────────────
# Tests
# ────────────────────────────────────────────────────────────────────────────────

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(test)
endif()