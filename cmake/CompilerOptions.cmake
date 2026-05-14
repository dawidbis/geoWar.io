# cmake/CompilerOptions.cmake
# Flagi kompilatora wspólne dla całego projektu.
# Używaj target_link_libraries(foo PRIVATE gs_compiler_options) zamiast
# set(CMAKE_CXX_FLAGS ...) — dzięki temu flagi nie przeciekają do FetchContent.

add_library(gs_compiler_options INTERFACE)
add_library(gs::compiler_options ALIAS gs_compiler_options)

# ── Wspólne flagi ─────────────────────────────────────────────────────────────
target_compile_options(gs_compiler_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wlogical-op
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /permissive-
        /w14640
    >
)

# ── Tryb Release ──────────────────────────────────────────────────────────────
target_compile_options(gs_compiler_options INTERFACE
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<CONFIG:Release>>:
        -O3
        -DNDEBUG
    >
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:
        /O2
    >
)

# ── Sanitizery (opcjonalne, -DGS_SANITIZERS=ON) ───────────────────────────────
if(GS_SANITIZERS)
    message(STATUS "[GS] Sanitizers enabled (ASan + UBSan)")
    target_compile_options(gs_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        >
    )
    target_link_options(gs_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=address,undefined
        >
    )
endif()

# ── Tryb debug / dev panel ────────────────────────────────────────────────────
if(GS_DEBUG_MODE)
    message(STATUS "[GS] Debug/dev panel enabled")
    target_compile_definitions(gs_compiler_options INTERFACE GS_DEBUG_MODE=1)
endif()
