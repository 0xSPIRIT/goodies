@echo off
cl.exe /Z7 /GS- /W4 /Gs9999999 /GR- /nologo goodies.cpp /link comdlg32.lib kernel32.lib user32.lib shell32.lib SDL2.lib SDL2_ttf.lib /incremental:no /NOIMPLIB /NOEXP /SUBSYSTEM:windows
