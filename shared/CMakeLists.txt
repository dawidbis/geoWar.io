# shared/CMakeLists.txt
# Serializer i Message są header-only (implementacja w .hpp).
# Biblioteka shared istnieje żeby eksportować include path i opcje kompilatora.

# Dummy .cpp — bez niego CMake nie może zbudować STATIC lib
set(GS_SHARED_DUMMY "${CMAKE_CURRENT_BINARY_DIR}/gs_shared_dummy.cpp")
file(WRITE "${GS_SHARED_DUMMY}" "// intentionally empty\n")

add_library(gs_shared STATIC "${GS_SHARED_DUMMY}")
add_library(gs::shared ALIAS gs_shared)

target_include_directories(gs_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(gs_shared
    PUBLIC
        gs::compiler_options
)

target_compile_features(gs_shared PUBLIC cxx_std_20)