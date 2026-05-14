# tests/CMakeLists.txt

# ── Auto-tworzenie brakujących plików testowych ───────────────────────────────
# Przy pierwszym checkout nie ma jeszcze żadnych testów — tworzymy placeholdery
# żeby CMake się nie poskarżył. Każdy placeholder ma jeden przechodzący test.

macro(gs_ensure_test_file REL_PATH)
    set(_full "${CMAKE_CURRENT_SOURCE_DIR}/${REL_PATH}")
    if(NOT EXISTS "${_full}")
        get_filename_component(_dir "${_full}" DIRECTORY)
        file(MAKE_DIRECTORY "${_dir}")
        get_filename_component(_name "${_full}" NAME_WE)
        file(WRITE "${_full}"
            "#include <gtest/gtest.h>\nTEST(${_name}, Placeholder) { EXPECT_TRUE(true); }\n"
        )
    endif()
endmacro()

gs_ensure_test_file(shared/test_message.cpp)
gs_ensure_test_file(shared/test_serializer.cpp)
gs_ensure_test_file(server/test_combat.cpp)
gs_ensure_test_file(server/test_population.cpp)
gs_ensure_test_file(server/test_supply.cpp)
gs_ensure_test_file(server/test_electric.cpp)
gs_ensure_test_file(server/test_annexation.cpp)
gs_ensure_test_file(server/test_worldgen.cpp)
gs_ensure_test_file(client/test_serializer_roundtrip.cpp)

# ── Helper ────────────────────────────────────────────────────────────────────
function(gs_add_test)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES;LIBS" ${ARGN})

    # Przelicz ścieżki relative to CMAKE_CURRENT_SOURCE_DIR
    set(_abs_sources)
    foreach(_src ${ARG_SOURCES})
        list(APPEND _abs_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endforeach()

    add_executable(${ARG_NAME} ${_abs_sources})

    target_link_libraries(${ARG_NAME}
        PRIVATE
            ${ARG_LIBS}
            gs::compiler_options
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
    )

    include(GoogleTest)
    gtest_discover_tests(${ARG_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

# ── Testy shared ──────────────────────────────────────────────────────────────
gs_add_test(NAME test_shared_message
    SOURCES shared/test_message.cpp
    LIBS gs::shared)

gs_add_test(NAME test_shared_serializer
    SOURCES shared/test_serializer.cpp
    LIBS gs::shared)

# ── Testy serwera ─────────────────────────────────────────────────────────────
if(GS_BUILD_SERVER)
    gs_add_test(NAME test_server_combat
        SOURCES server/test_combat.cpp
        LIBS gs::server_lib)
    gs_add_test(NAME test_server_population
        SOURCES server/test_population.cpp
        LIBS gs::server_lib)
    gs_add_test(NAME test_server_supply
        SOURCES server/test_supply.cpp
        LIBS gs::server_lib)
    gs_add_test(NAME test_server_electric
        SOURCES server/test_electric.cpp
        LIBS gs::server_lib)
    gs_add_test(NAME test_server_annexation
        SOURCES server/test_annexation.cpp
        LIBS gs::server_lib)
    gs_add_test(NAME test_server_worldgen
        SOURCES server/test_worldgen.cpp
        LIBS gs::server_lib)
endif()

# ── Testy klienta ─────────────────────────────────────────────────────────────
if(GS_BUILD_CLIENT)
    gs_add_test(NAME test_client_serializer_roundtrip
        SOURCES client/test_serializer_roundtrip.cpp
        LIBS gs::client_lib)
endif()