# cmake/FetchDependencies.cmake

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ── Boost ─────────────────────────────────────────────────────────────────────
if(GS_BUILD_SERVER OR GS_BUILD_CLIENT)
    find_package(Boost 1.70 REQUIRED)
    if(NOT Boost_FOUND)
        message(FATAL_ERROR "[GS] Boost not found. Set BOOST_ROOT.")
    endif()
    message(STATUS "[GS] Boost found: ${Boost_VERSION}")

    add_library(gs_boost_asio INTERFACE)
    add_library(gs::boost_asio ALIAS gs_boost_asio)
    target_include_directories(gs_boost_asio INTERFACE ${Boost_INCLUDE_DIRS})
    target_compile_definitions(gs_boost_asio INTERFACE
        BOOST_ASIO_NO_DEPRECATED=1
        $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0601>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    )
endif()

# ── SFML ──────────────────────────────────────────────────────────────────────
if(GS_BUILD_CLIENT)
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        2.6.1
        GIT_SHALLOW    TRUE)
    set(SFML_BUILD_AUDIO   OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SFML)
    message(STATUS "[GS] SFML fetched")
endif()

# ── Dear ImGui ────────────────────────────────────────────────────────────────
if(GS_BUILD_CLIENT)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.90.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(imgui)

    # imgui-sfml — pobierz tylko pliki źródłowe (FetchContent_Populate nie buduje)
    FetchContent_Declare(imgui_sfml
        GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
        GIT_TAG        v2.6
        GIT_SHALLOW    TRUE)
    FetchContent_GetProperties(imgui_sfml)
    if(NOT imgui_sfml_POPULATED)
        FetchContent_Populate(imgui_sfml)
    endif()

    add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_sfml_SOURCE_DIR}/imgui-SFML.cpp
    )

    target_include_directories(imgui_lib
        PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
            ${imgui_sfml_SOURCE_DIR}
    )

    target_link_libraries(imgui_lib PUBLIC sfml-graphics sfml-window sfml-system)

    target_compile_definitions(imgui_lib PUBLIC
        $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0601>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    )

    add_library(gs::imgui ALIAS imgui_lib)
    message(STATUS "[GS] Dear ImGui + imgui-SFML fetched")
endif()

# ── GoogleTest ────────────────────────────────────────────────────────────────
if(GS_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    message(STATUS "[GS] GoogleTest fetched")
endif()