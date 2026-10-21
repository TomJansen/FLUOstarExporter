@echo off
setlocal
cd /d "%~dp0\.."
if "%LLVM_MINGW_MSVCRT%"=="" (
 echo Set LLVM_MINGW_MSVCRT to the extracted LLVM-MinGW MSVCRT toolchain directory.
 exit /b 1
)
if not exist build mkdir build
if not exist dist mkdir dist
"%LLVM_MINGW_MSVCRT%\bin\i686-w64-mingw32-windres.exe" -Isrc src\app.rc -o build\app-res.o
if errorlevel 1 exit /b 1
"%LLVM_MINGW_MSVCRT%\bin\i686-w64-mingw32-clang++.exe" -std=c++17 -O2 -Wall -Wextra -Werror -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -municode -mwindows -static src\app.cpp build\app-res.o -o dist\FLUOstarExporter.exe -lcomctl32 -lole32 -lshell32 -luuid -Wl,--major-subsystem-version,6,--minor-subsystem-version,1 -Wl,-s
if errorlevel 1 exit /b 1
python tools\package.py --objdump "%LLVM_MINGW_MSVCRT%\bin\llvm-objdump.exe"
if errorlevel 1 exit /b 1
echo Built dist\FLUOstarExporter.exe and FLUOstarExporter-portable.zip
