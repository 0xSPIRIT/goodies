@echo off
cl.exe /Z7 /GS- /W4 /EHa- /GR- /nologo goodies.cpp /link shell32.lib comdlg32.lib user32.lib SDL2.lib SDL2_ttf.lib SDL2_image.lib /incremental:no
