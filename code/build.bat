@echo off

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
:: call "w:\handmade\misc\shell.bat"
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl -MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7 -Fmwin32_handmade.map ..\handmade\code\win32_handmade.cpp /link -opt:ref user32.lib gdi32.lib
popd