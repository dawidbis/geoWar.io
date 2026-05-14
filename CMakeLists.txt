cmake_minimum_required(VERSION 3.25)

project(GrandStrategy
    VERSION 0.1.0
    DESCRIPTION "2D networked grand strategy game"
    LANGUAGES CXX
)

# ── Standardy i opcje globalne ────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Eksport compile_commands.json dla clangd / IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Opcje budowania ────────────────────────────────────────────────────────────
option(GS_BUILD_SERVER  "Build the server executable"   ON)
option(GS_BUILD_CLIENT  "Build the client executable"   ON)
option(GS_BUILD_TESTS   "Build tests"                   ON)
option(GS_DEBUG_MODE    "Enable debug/dev panel in client" OFF)
option(GS_SANITIZERS    "Enable AddressSanitizer + UBSan" OFF)

# ── Moduły cmake/ ─────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerOptions)
include(FetchDependencies)

# ── Podprojekty ───────────────────────────────────────────────────────────────
add_subdirectory(shared)

if(GS_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(GS_BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(GS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
