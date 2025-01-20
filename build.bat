cl /std:c17 /Zi /O2 /GL /Qpar .\ecosystem.c /I"./external/inc" /link user32.lib gdi32.lib /OUT:ecosystem.exe
cl /std:c17 /Zi /O2 /GL /Qpar .\util\util_json_test.c /link /OUT:.\util\util_json_test.exe