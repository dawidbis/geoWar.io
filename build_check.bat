@echo off
echo ===================================
echo Rozpoczynam sprawdzanie kompilacji...
echo ===================================

:: Krok 1: Generowanie plików (tylko jeśli struktura uległa zmianie, ale warto zostawić)
cmake -S . -B build

:: Krok 2: Właściwa kompilacja
cmake --build build --config Debug

:: Krok 3: Sprawdzenie wyniku
if %ERRORLEVEL% equ 0 (
    echo.
    echo ===================================
    echo [SUKCES] Kod skompilowal sie bez bledow!
    echo ===================================
) else (
    echo.
    echo ===================================
    echo [BLAD] Kompilacja zakonczona niepowodzeniem.
    echo ===================================
)
pause