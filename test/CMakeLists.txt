include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

# ── Sim slot ───────────────────────────────────────────────────────────────────
add_library(slot_sim STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/slot_sim.c
)
target_include_directories(slot_sim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# ── Helper to register a test executable ──────────────────────────────────────
function(add_iso7816_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE iso7816_static slot_sim unity)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/protocols
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_iso7816_test(test_atr)
add_iso7816_test(test_pps)
add_iso7816_test(test_apdu_t0)
add_iso7816_test(test_apdu_t1)
