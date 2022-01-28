@echo off
%~dp0fxc.exe "%~dp0shaders.hlsl" /Od /Zi /T vs_5_1 /E "VSMain" /Fo "%~dp0vs.cso" /Fc "%~dp0vs.asm"
%~dp0fxc.exe "%~dp0shaders.hlsl" /Od /Zi /T ps_5_1 /E "PSMain" /Fo "%~dp0ps.cso" /Fc "%~dp0ps.asm"