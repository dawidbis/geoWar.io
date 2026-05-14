# cmake/FetchDependencies.cmake

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ── Boost (serwer) ────────────────────────────────────────────────────────────
if(GS_BUILD_SERVER)
    # MODULE mode — działa z ręczną instalacją (sourceforge precompiled binaries)
    # Asio jest header-only od Boost 1.70, nie potrzebujemy kompilowanego system
    find_package(Boost 1.70 REQUIRED)

    if(NOT Boost_FOUND)
        message(FATAL_ERROR
            "[GS] Boost not found.\n"
            "Ustaw BOOST_ROOT na folder instalacji, np:\n"
            "  cmake -B build -DBOOST_ROOT=C:/local/boost_1_91_0\n"
        )
    endif()

    message(STATUS "[GS] Boost found: ${Boost_VERSION} at ${Boost_INCLUDE_DIRS}")

    # Header-only interface target — Asio nie wymaga linkowania Boost.System
    add_library(gs_boost_asio INTERFACE)
    add_library(gs::boost_asio ALIAS gs_boost_asio)
    target_include_directories(gs_boost_asio INTERFACE ${Boost_INCLUDE_DIRS})
    target_compile_definitions(gs_boost_asio INTERFACE
        BOOST_ASIO_NO_DEPRECATED=1
        BOOST_ASIO_STANDALONE       # nie wymaga Boost.System
    )
endif()

# ── SFML (klient) ─────────────────────────────────────────────────────────────
if(GS_BUILD_CLIENT)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        2.6.1
        GIT_SHALLOW    TRUE
    )
    set(SFML_BUILD_AUDIO   OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(SFML)
    message(STATUS "[GS] SFML fetched")
endif()

# ── Dear ImGui (klient) ───────────────────────────────────────────────────────
if(GS_BUILD_CLIENT)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.90.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_lib PUBLIC sfml-graphics sfml-window)
    add_library(gs::imgui ALIAS imgui_lib)
    message(STATUS "[GS] Dear ImGui fetched")
endif()

# ── GoogleTest (testy) ────────────────────────────────────────────────────────
if(GS_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)
    message(STATUS "[GS] GoogleTest fetched")
endif()