@echo off
title Grand Strategy SERVER
echo ===========================================
echo Uruchamianie serwera Grand Strategy...
echo Port domyslny: 7777
echo ===========================================

if exist "build\server\Debug\gs_server.exe" (
    build\server\Debug\gs_server.exe
) else (
    echo [BLAD] Nie znaleziono pliku gs_server.exe. 
    echo Upewnij sie, ze projekt zostal poprawnie skompilowany.
)

echo.
echo Serwer zostal zatrzymany.
pause