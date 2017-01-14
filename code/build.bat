@echo off
mkdir ..\build
pushd ..\build
cl -DHANDMADE_WIN32=1 -FC -Zi ..\code\win32_handmade.cpp user32.lib gdi32.lib
popd
