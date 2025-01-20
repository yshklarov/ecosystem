@echo off

mkdir build
pushd build
cl /std:c17 /Zi /O2 /GL /Qpar ..\src\ecosystem.c /I"..\external\inc" /link user32.lib gdi32.lib /OUT:ecosystem.exe
cl /std:c17 /Zi /O2 /GL /Qpar ..\src\util_json_test.c /link /OUT:util_json_test.exe
popd
