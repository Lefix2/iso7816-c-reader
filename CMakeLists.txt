cmake_minimum_required(VERSION 3.16)
project(iso7816 VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SC_MAX_SLOTS 2 CACHE STRING "Maximum number of registered smartcard slots")

# ── Sources ────────────────────────────────────────────────────────────────────
set(ISO7816_SOURCES
    src/smartcard.c
    src/sc_defs.c
    src/maths/EDC.c
    src/protocols/protocol_atr.c
    src/protocols/protocol_pps.c
    src/protocols/protocol_TPDU_T0.c
    src/protocols/protocol_APDU_T0.c
    src/protocols/protocol_TPDU_T1.c
    src/protocols/protocol_APDU_T1.c
)

set(ISO7816_INCLUDE_DIRS
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

set(ISO7816_COMPILE_OPTIONS
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
)

# ── Object library (compile once, link into both static and shared) ────────────
add_library(iso7816_objects OBJECT ${ISO7816_SOURCES})
set_target_properties(iso7816_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(iso7816_objects PUBLIC ${ISO7816_INCLUDE_DIRS})
target_compile_options(iso7816_objects PRIVATE ${ISO7816_COMPILE_OPTIONS})
target_compile_definitions(iso7816_objects PUBLIC SC_MAX_SLOTS=${SC_MAX_SLOTS})

# ── Static library ─────────────────────────────────────────────────────────────
add_library(iso7816_static STATIC $<TARGET_OBJECTS:iso7816_objects>)
add_library(iso7816::static ALIAS iso7816_static)
target_include_directories(iso7816_static PUBLIC ${ISO7816_INCLUDE_DIRS})
set_target_properties(iso7816_static PROPERTIES OUTPUT_NAME iso7816)

# ── Shared library ─────────────────────────────────────────────────────────────
add_library(iso7816_shared SHARED $<TARGET_OBJECTS:iso7816_objects>)
add_library(iso7816::shared ALIAS iso7816_shared)
target_include_directories(iso7816_shared PUBLIC ${ISO7816_INCLUDE_DIRS})
set_target_properties(iso7816_shared PROPERTIES OUTPUT_NAME iso7816)

# ── Install ────────────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS iso7816_static iso7816_shared
    EXPORT iso7816Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    include/smartcard.h
    include/slot_itf.h
    include/sc_defs.h
    include/sc_status.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iso7816
)

# ── Tests ──────────────────────────────────────────────────────────────────────
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(test)
    endif()
endif()
