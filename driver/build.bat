@echo off

echo =======================================================
echo Building Potassium Driver (x64)
echo =======================================================

set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community
set MSVC_BIN=%VS_PATH%\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64
set WDK_INC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.28000.0
set WDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.28000.0\km\x64

cd /d "%~dp0"

echo [*] Compiling main.c ...
"%MSVC_BIN%\cl.exe" /c /nologo /W3 /O2 /Oi /kernel /GS- /I"%WDK_INC%\km" /I"%WDK_INC%\km\crt" /I"%WDK_INC%\shared" /I"%WDK_INC%\ucrt" /I"%~dp0." /D_AMD64_ /Fo"main.obj" "main.c"

if errorlevel 1 (
    echo [!] Compilation failed!
    exit /b 1
)

echo [*] Linking driver.sys ...
"%MSVC_BIN%\link.exe" /NOLOGO /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /NODEFAULTLIB /OUT:"driver.sys" /LIBPATH:"%WDK_LIB%" ntoskrnl.lib "main.obj"

if errorlevel 1 (
    echo [!] Linking failed!
    exit /b 1
)

echo [+] Successfully built driver.sys!
echo =======================================================
