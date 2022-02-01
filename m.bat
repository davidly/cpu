del cpu.exe
del cpu.pdb
rc cpu.rc
cl /nologo cpu.cxx /I.\ /Ox /Qpar /O2 /Oi /Ob2 /EHac /Zi /Gy /D_AMD64_ /link ntdll.lib user32.lib gdi32.lib cpu.res /OPT:REF /subsystem:windows


