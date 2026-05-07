include(FetchContent)

# ────────────────────────────────────────────────────────────────────────────────
# Unity
# ────────────────────────────────────────────────────────────────────────────────

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(unity)

# ────────────────────────────────────────────────────────────────────────────────
# Slot simulator
# ────────────────────────────────────────────────────────────────────────────────

add_library(slot_sim STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/slot_sim.c
)

target_include_directories(slot_sim
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(slot_sim
    PRIVATE
        iso7816_warnings
)

# ────────────────────────────────────────────────────────────────────────────────
# Test helper
# ────────────────────────────────────────────────────────────────────────────────

function(add_iso7816_test name include_private)

    add_executable(${name} ${name}.c)

    target_link_libraries(${name}
        PRIVATE
            iso7816_static
            slot_sim
            unity
            iso7816_warnings
            iso7816_sanitizers
            iso7816_coverage
    )

    if(include_private)
        target_include_directories(${name}
            PRIVATE
                ${PROJECT_SOURCE_DIR}/src/include
        )
    endif()

    add_test(NAME ${name} COMMAND ${name})

    set_tests_properties(${name}
        PROPERTIES
            LABELS "unit"
            TIMEOUT 5
    )

endfunction()

# ────────────────────────────────────────────────────────────────────────────────
# Tests
# ────────────────────────────────────────────────────────────────────────────────

add_iso7816_test(test_edc TRUE)
add_iso7816_test(test_atr TRUE)
add_iso7816_test(test_pps TRUE)
add_iso7816_test(test_apdu_t0 TRUE)
add_iso7816_test(test_apdu_t1 TRUE)
add_iso7816_test(test_tpdu_t0 TRUE)
add_iso7816_test(test_tpdu_t1 TRUE)

add_iso7816_test(test_smartcard FALSE)