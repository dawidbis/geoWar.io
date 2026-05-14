# server/CMakeLists.txt

file(GLOB_RECURSE GS_SERVER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(NOT GS_SERVER_SOURCES)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gs_server_dummy.cpp" "// placeholder\n")
    set(GS_SERVER_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/gs_server_dummy.cpp")
endif()

add_executable(gs_server ${GS_SERVER_SOURCES} "src/net/Session.cpp" "src/net/GameServer.cpp" "src/main.cpp")
add_executable(gs::server ALIAS gs_server)

target_include_directories(gs_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(gs_server PRIVATE gs::shared gs::compiler_options gs::boost_asio)

# ── Serwer jako biblioteka (do testów) ────────────────────────────────────────
file(GLOB_RECURSE GS_SERVER_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER GS_SERVER_LIB_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

if(NOT GS_SERVER_LIB_SOURCES)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gs_server_lib_dummy.cpp" "// placeholder\n")
    set(GS_SERVER_LIB_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/gs_server_lib_dummy.cpp")
endif()

add_library(gs_server_lib STATIC ${GS_SERVER_LIB_SOURCES} "src/net/Session.cpp" "src/net/GameServer.cpp" "src/main.cpp")
add_library(gs::server_lib ALIAS gs_server_lib)

target_include_directories(gs_server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gs_server_lib PUBLIC gs::shared gs::compiler_options gs::boost_asio)