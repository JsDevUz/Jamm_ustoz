@echo off
REM "x64 Native Tools Command Prompt for VS" (yoki Developer Command Prompt) ichida ishga tushiring.
cl /nologo /std:c++17 /EHsc /O2 /W4 /MT /DUNICODE /D_UNICODE main.cpp /Fe:JammHost.exe /link /SUBSYSTEM:WINDOWS d2d1.lib dwrite.lib user32.lib
if errorlevel 1 (echo BUILD XATOSI & exit /b 1)
echo Tayyor: JammHost.exe
