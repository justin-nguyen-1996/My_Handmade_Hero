@echo off
IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
cl -MT -Gm- -nologo -GR- -EHa- -Od -Oi -W4 -wd4201 -wd4100 -wd4101 -wd4189 -wd4800 -DHANDMADE_WIN32=1 -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -FC -Z7 -Fmwin32_handmade.map ..\code\win32_handmade.cpp /link -opt:ref -subsystem:windows,5.1 user32.lib gdi32.lib
popd
