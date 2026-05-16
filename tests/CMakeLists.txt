# tests/CMakeLists.txt

# --- DODAJ TĘ LINIJKĘ ---
# Wczytuje moduł zawierający komendę gtest_discover_tests
include(GoogleTest)

# --- Twój stary test dla shared ---
add_executable(test_serializer shared/test_serializer.cpp)
target_link_libraries(test_serializer PRIVATE gs::shared GTest::gtest_main)
gtest_discover_tests(test_serializer)

# --- NOWE TESTY DLA SERWERA ---

# Zbieramy wszystkie pliki .cpp z folderu server/
file(GLOB_RECURSE SERVER_TEST_SOURCES CONFIGURE_DEPENDS "server/*.cpp")

add_executable(test_server ${SERVER_TEST_SOURCES})

# Linkujemy nasz silnik gry (gs::server_lib) oraz GTest
target_link_libraries(test_server PRIVATE 
    gs::server_lib 
    GTest::gtest_main
)
# --- STRESS TEST ---
# Zmieniona ścieżka: plik leży teraz prosto w folderze tests/
add_executable(stress_test stress_test.cpp) 
target_link_libraries(stress_test PRIVATE 
    gs::shared 
    gs::boost_asio
)

# Rejestrujemy testy, aby CTest je widział
gtest_discover_tests(test_server)