@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo Building Potassium.dll (Internal Execution Module)
echo =======================================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

cl.exe /O2 /W3 /std:c++17 /EHsc /MD /D_USRDLL /D_WINDLL ^
    roblox_encoder.cpp filesystem_api.cpp crypt.cpp drawing.cpp ^
    websocket.cpp teleport.cpp script_receiver.cpp environment.cpp dllmain.cpp ^
    /link /DLL /OUT:Potassium.dll ws2_32.lib wininet.lib bcrypt.lib user32.lib advapi32.lib

if %ERRORLEVEL% equ 0 (
    echo [SUCCESS] Potassium.dll compiled successfully!
) else (
    echo [ERROR] Compilation failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
