@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo Building Decompiler.exe (Luau Decompiler Service)
echo =======================================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

cl.exe /O2 /W3 /std:c++17 /EHsc /MD /D_CONSOLE ^
    decompiler.cpp server.cpp main.cpp ^
    /link /SUBSYSTEM:CONSOLE /OUT:Decompiler.exe ws2_32.lib

if %ERRORLEVEL% equ 0 (
    echo [SUCCESS] Decompiler.exe compiled successfully!
) else (
    echo [ERROR] Compilation failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
