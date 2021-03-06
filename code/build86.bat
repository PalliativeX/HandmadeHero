@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4459 -wd4456 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
call cls   REM Used for cleaning up the terminal

cl  %CommonCompilerFlags% ..\handmade\code\handmade.cpp -Fmhandmade.map /LD /link /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
cl  %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%

popd