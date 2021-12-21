@echo off
cmake -G "Visual Studio 16 2019" -S . -B build
cmake --build build --target learndx12
.\target\Debug\learndx12.exe