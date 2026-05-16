# client/CMakeLists.txt

file(GLOB_RECURSE GS_CLIENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

if(NOT GS_CLIENT_SOURCES)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gs_client_dummy.cpp" "// placeholder\n")
    set(GS_CLIENT_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/gs_client_dummy.cpp")
endif()

add_executable(gs_client ${GS_CLIENT_SOURCES})
add_executable(gs::client ALIAS gs_client)

target_include_directories(gs_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(gs_client
    PRIVATE
        gs::shared
        gs::compiler_options
        gs::boost_asio
        sfml-graphics sfml-window sfml-system
        $<$<PLATFORM_ID:Windows>:sfml-main>
        gs::imgui
        $<$<PLATFORM_ID:Linux>:GL>
        $<$<PLATFORM_ID:Darwin>:"-framework OpenGL">
        $<$<PLATFORM_ID:Windows>:opengl32>
)

if(GS_DEBUG_MODE)
    target_compile_definitions(gs_client PRIVATE GS_DEBUG_MODE=1)
endif()

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(gs_client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── Klient jako biblioteka (do testów) ────────────────────────────────────────
set(GS_CLIENT_LIB_SOURCES ${GS_CLIENT_SOURCES})
list(FILTER GS_CLIENT_LIB_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

if(NOT GS_CLIENT_LIB_SOURCES)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gs_client_lib_dummy.cpp" "// placeholder\n")
    set(GS_CLIENT_LIB_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/gs_client_lib_dummy.cpp")
endif()

add_library(gs_client_lib STATIC ${GS_CLIENT_LIB_SOURCES})
add_library(gs::client_lib ALIAS gs_client_lib)

target_include_directories(gs_client_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gs_client_lib
    PUBLIC
        gs::shared gs::compiler_options gs::boost_asio
        sfml-graphics sfml-window sfml-system
        $<$<PLATFORM_ID:Windows>:sfml-main>
        gs::imgui
        $<$<PLATFORM_ID:Linux>:GL>
        $<$<PLATFORM_ID:Darwin>:"-framework OpenGL">
        $<$<PLATFORM_ID:Windows>:opengl32>
)