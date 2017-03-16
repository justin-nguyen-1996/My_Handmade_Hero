@echo off

set commonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4505 -wd4201 -wd4100 -wd4101 -wd4189 -wd4800 -DHANDMADE_WIN32=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7  
set commonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

del .*~
del *.*~
del *~

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

del *.pdb > NUL 2> NUL

REM 32-bit build
REM cl %commonCompilerFlags% ..\code\win32_handmade.cpp /link -subsystem:windows,5.1 %commonLinkerFlags%

REM 64-bit build
cl %CommonCompilerFlags% ..\code\handmade.cpp -Fmhandmade.map -LD /link -incremental:no -opt:ref -PDB:handmade_%random%.pdb -EXPORT:gameGetSoundSamples -EXPORT:gameUpdateAndRender
cl %commonCompilerFlags% ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %commonLinkerFlags%

popd
