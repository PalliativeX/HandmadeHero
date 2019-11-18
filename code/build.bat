@echo off

mkdir ..\..\build
pushd ..\..\build
cl -Zi ..\HandmadeHero\code\win32_handmade.cpp user32.lib
popd