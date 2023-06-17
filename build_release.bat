@echo off
cl.exe /O2 /GS- /W4 /Gs9999999 /GR- /nologo goodies.cpp /link SDL2.lib SDL2_ttf.lib /incremental:no /NOIMPLIB /NOEXP