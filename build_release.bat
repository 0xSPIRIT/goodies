@echo off
cl.exe /DRELEASE /Z7 /O2 /GS- /GR- /EHa- /W4 /nologo goodies.cpp /link shell32.lib comdlg32.lib user32.lib SDL2.lib SDL2_ttf.lib SDL2_image.lib /incremental:no /SUBSYSTEM:windows
