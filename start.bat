@echo off
rem set environmental variables for toolchain (TODO)
rem - MASM611C can be found in folder toolset of this project
rem - furthermore MS Visual Studio 6 is required (not included in this project)
set MASM=C:\DDK\MASM611C
set MSVC=C:\PROGRA~1\MICROS~2\VC98\BIN
set MSVCOMMON=C:\PROGRA~1\MICROS~2\COMMON\MSDEV98\BIN
set PATH=%MSVCOMMON%;%MSVC%;%MASM%;%PATH%
doskey
