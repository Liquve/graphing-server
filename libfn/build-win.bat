@echo off
setlocal

echo Building...

REM --- если передан аргумент — считаем его путём к mingw/bin
if not "%~1"=="" (
    set "MINGW_BIN=%~1"
) else (
    set "MINGW_BIN="
)

REM --- если путь задан, добавляем в PATH
if defined MINGW_BIN (
    if exist "%MINGW_BIN%\gcc.exe" (
        set "PATH=%MINGW_BIN%;%PATH%"
    ) else (
        echo ERROR: gcc.exe not found in "%MINGW_BIN%"
        pause
        exit /b 1
    )
)

REM --- проверяем gcc
where gcc >nul 2>&1
if errorlevel 1 (
    echo ERROR: gcc not found. Pass MinGW path as argument.
    echo Example:
    echo build-win.bat C:\Qt\Tools\mingw1310_64\bin
    pause
    exit /b 1
)

REM --- чистим
del /f /q libfn.dll >nul 2>&1
del /f /q main.exe >nul 2>&1

REM --- сборка DLL
echo [1/2] Building libfn.dll...
gcc -shared libfn.c -o libfn.dll -lm
if errorlevel 1 (
    echo ERROR: failed to build libfn.dll
    pause
    exit /b 1
)

REM --- сборка exe
echo [2/2] Building main.exe...
gcc main.c -o main.exe -lm
if errorlevel 1 (
    echo ERROR: failed to build main.exe
    pause
    exit /b 1
)

echo Building completed

