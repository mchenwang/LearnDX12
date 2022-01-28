@echo off
set para="%1"
if %para% == "build" (
    cmake -G "Visual Studio 16 2019" -S . -B build
    cmake --build build --target learndx12
) else (
    cmake --build build --target learndx12
)
.\target\Debug\learndx12.exe