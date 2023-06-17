@echo off
cl.exe /Z7 /GS- /W4 /Gs9999999 /GR- /nologo goodies.cpp /link shell32.lib comdlg32.lib SDL2.lib SDL2_ttf.lib /incremental:no /NOIMPLIB /NOEXP
