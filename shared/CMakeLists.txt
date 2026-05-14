# shared/CMakeLists.txt

file(GLOB_RECURSE GS_SHARED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Jeśli nie ma jeszcze żadnych .cpp — tworzymy dummy żeby CMake się nie poskarżył
if(NOT GS_SHARED_SOURCES)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gs_shared_dummy.cpp"
        "// placeholder — usunięty automatycznie gdy pojawią się prawdziwe źródła\n"
    )
    set(GS_SHARED_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/gs_shared_dummy.cpp")
endif()

add_library(gs_shared STATIC ${GS_SHARED_SOURCES} "src/net/Serializer.cpp")
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
