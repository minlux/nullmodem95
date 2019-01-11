@echo off

rem Compile C Files (IS_32 ^= 32-bit instruction set)
cl -nologo -c -FA -DVXD -DIS_32 -I.\inc32 .\src\driver.c
cl -nologo -c -FA -DVXD -DIS_32 -I.\inc32 .\src\stdutils.c


rem Assemble ASM Files
ml -coff -c -Cx -DMASM6 -DBLD_COFF -DIS_32 -I.\inc32 .\src\mxvcp.asm


rem Link to VXD
link -vxd -nodefaultlib -def:.\src\mxvcp.def -out:.\result\mxvcp.vxd mxvcp.obj driver.obj stdutils.obj
