@echo off
title Grand Strategy CLIENT
echo ===========================================
echo Uruchamianie klienta Grand Strategy...
echo Adres: 127.0.0.1
echo ===========================================

if exist "build\client\Debug\gs_client.exe" (
    :: Uzywamy start, aby konsola skryptu nie blokowala sie
    start build\client\Debug\gs_client.exe
) else (
    echo [BLAD] Nie znaleziono pliku gs_client.exe.
    echo Upewnij sie, ze projekt zostal poprawnie skompilowany.
    pause
)