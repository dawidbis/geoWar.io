@echo off
echo ===================================
echo Uruchamianie testow (CTest)...
echo ===================================

:: Przejscie do folderu build (tam gdzie CMake wygenerowal pliki)
cd build

:: Uruchomienie testow w konfiguracji Debug. 
:: Flaga --output-on-failure sprawi, ze jesli test nie przejdzie, zobaczysz dokladny blad z GTest.
ctest -C Debug --output-on-failure

:: Sprawdzenie wyniku
if %ERRORLEVEL% equ 0 (
    echo.
    echo ===================================
    echo [SUKCES] Wszystkie testy przeszly wzorowo!
    echo ===================================
) else (
    echo.
    echo ===================================
    echo [BLAD] Niestety, niektore testy "oblaly". Sprawdz logi wyzej.
    echo ===================================
)

:: Powrot do glownego folderu
cd ..
pause