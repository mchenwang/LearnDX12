@echo off
if %1 == 1 (
    cmake -G "Visual Studio 16 2019" -S . -B build
    cmake --build build --target learndx12
) else if %1 == 2 (
    cmake --build build --target learndx12
)
.\target\Debug\learndx12.exe