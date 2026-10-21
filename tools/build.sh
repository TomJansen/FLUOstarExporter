#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build dist
# Use the MSVCRT variant: UCRT builds need an extra runtime on older Windows.
: "${LLVM_MINGW_MSVCRT:?Set LLVM_MINGW_MSVCRT to the extracted LLVM-MinGW MSVCRT toolchain}"
# Linux distributions include a native compiler driver and tools in the archive.
# macOS can use Apple Clang with the Windows toolchain's headers/libraries.
native_driver="$LLVM_MINGW_MSVCRT/bin/i686-w64-mingw32-clang++"
if [[ -x "$native_driver" && -z "${CROSSOVER_WINE:-}" ]]; then
 "$LLVM_MINGW_MSVCRT/bin/i686-w64-mingw32-windres" -Isrc src/app.rc -o build/app-res.o
 "$native_driver" -std=c++17 -O2 -Wall -Wextra -Werror \
  -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -municode -mwindows -static \
  src/app.cpp build/app-res.o -o dist/FLUOstarExporter.exe \
  -lcomctl32 -lole32 -lshell32 -luuid \
  -Wl,--major-subsystem-version,6,--minor-subsystem-version,1
 "$LLVM_MINGW_MSVCRT/bin/llvm-strip" dist/FLUOstarExporter.exe
 python3 tools/package.py --objdump "$LLVM_MINGW_MSVCRT/bin/llvm-objdump"
 exit 0
fi
compiler="${CXX_WINDOWS:-clang++}"
linker="${LD_WINDOWS:-ld.lld}"
crt="$LLVM_MINGW_MSVCRT/i686-w64-mingw32/lib"
builtins="$LLVM_MINGW_MSVCRT/lib/clang/19/lib/windows"
i686-w64-mingw32-windres -Isrc src/app.rc -o build/app-res.o
"$compiler" --target=i686-w64-windows-gnu --sysroot="$LLVM_MINGW_MSVCRT" \
 -isystem "$LLVM_MINGW_MSVCRT/include/c++/v1" \
 -isystem "$LLVM_MINGW_MSVCRT/i686-w64-mingw32/include" \
 -std=c++17 -O2 -Wall -Wextra -Werror -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 \
 -c src/app.cpp -o build/app-legacy.o
# LLVM's linker is required for the LLVM i686 exception runtime.
# On macOS, optionally run the Windows driver from an isolated CrossOver bottle.
if [[ -n "${CROSSOVER_WINE:-}" ]]; then
 "$CROSSOVER_WINE" --bottle "${CROSSOVER_BOTTLE:-FluoExporterTest}" \
  "$LLVM_MINGW_MSVCRT/bin/i686-w64-mingw32-clang++.exe" \
  -municode -mwindows -static build/app-legacy.o build/app-res.o \
  -o dist/FLUOstarExporter.exe -lcomctl32 -lole32 -lshell32 -luuid \
  -Wl,--major-subsystem-version,6,--minor-subsystem-version,1
else
 "$linker" -static -m i386pe --subsystem windows:6.1 --entry WinMainCRTStartup \
  -o dist/FLUOstarExporter.exe "$crt/crt2u.o" "$crt/crtbegin.o" \
  build/app-legacy.o build/app-res.o -L"$crt" -L"$builtins" --start-group \
  -lc++ -lc++abi -lunwind -lmingw32 -lclang_rt.builtins-i386 -lmingwex -lmsvcrt \
  -lkernel32 -luser32 -lgdi32 -lcomctl32 -lole32 -lshell32 -luuid -ladvapi32 \
  --end-group "$crt/crtend.o"
fi
i686-w64-mingw32-strip dist/FLUOstarExporter.exe
python3 tools/package.py --objdump "${OBJDUMP_WINDOWS:-i686-w64-mingw32-objdump}"
