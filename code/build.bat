@echo off

set commonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4101 -wd4189 -wd4800 -DHANDMADE_WIN32=1 -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -FC -Z7 -Fmwin32_handmade.map 
set commonLinkerFlags=-opt:ref user32.lib gdi32.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 32-bit build
REM cl %commonCompilerFlags% ..\code\win32_handmade.cpp /link -subsystem:windows,5.1 %commonLinkerFlags%

REM 64-bit build
cl %commonCompilerFlags% ..\code\win32_handmade.cpp /link %commonLinkerFlags%

popd
