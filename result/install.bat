@echo off
rem CREATE BACKUP DIRECTORY, if is doesn't exist yet
rem   batch can only check for existing files. so i use a trick.
rem   each directory contains a couple of pseudo files. here, i check
rem   for the pseudo file "nul" to check the existance of directory
IF NOT EXIST bak\nul md bak

rem BACKUP REGISTRY KEYS, if they doesn't exist yet
IF NOT EXIST "bak\hports.reg" regedit /E bak\hports.reg "HKEY_LOCAL_MACHINE\Enum\Root\Ports"
IF NOT EXIST "bak\sports.reg" regedit /E bak\sports.reg "HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Ports"

rem CHECK existance of "windows-path"
IF "%windir%"=="" goto FAIL
rem COPY VXD DRIVER and INSTALL COM PORTS
copy mxvcp.vxd %windir%\\system /Y
regedit /S com3456.reg
echo Driver and COM ports COM3, COM4, COM5, COM6 installed!
goto END

:FAIL
echo Can't install, as environment variable "windir" is not set!

:END
