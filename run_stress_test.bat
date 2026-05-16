@echo off
title Grand Strategy - Stress Test
color 0A

echo ===================================================
echo        GRAND STRATEGY - NARZEDZIE STRESS TEST
echo ===================================================
echo.
echo Upewnij sie, ze serwer (gs_server.exe) jest juz uruchomiony w innym oknie!
echo.
pause

echo.
echo Szukanie pliku wykonywalnego...

:: Œcie¿ka dla Visual Studio / MSVC (domyœlnie tworzy podfolder Debug)
set EXE_PATH="build\tests\Debug\stress_test.exe"

if exist %EXE_PATH% (
    echo Znaleziono stress_test.exe! Odpalanie 50 botow...
    echo ===================================================
    %EXE_PATH%
) else (
    color 0C
    echo [BLAD] Nie znaleziono pliku: %EXE_PATH%
    echo Upewnij sie, ze projekt zostal pomyslnie skompilowany.
)

echo.
pause