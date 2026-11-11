@echo off
rem Builds the ESWC spike: dist\legacy_randomizer.dll and the test loader dist\dinput8.dll (x86, MSVC Build Tools).
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
if not exist "%VCVARS%" goto novc
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
if not exist dist mkdir dist
cl /nologo /c /O1 /GS- /W4 /WX /EHs-c- /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /Fo"build\\" src\testloader.cpp || exit /b 1
link /nologo /DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /NODEFAULTLIB /ENTRY:DllMain /DEF:src\testloader.def /OUT:dist\dinput8.dll /IMPLIB:build\dinput8.lib build\testloader.obj user32.lib kernel32.lib || exit /b 1
cl /nologo /c /O2 /MT /GS /W4 /WX /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /Fo"build\\" src\spike_eswc.cpp || exit /b 1
link /nologo /DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /OUT:dist\legacy_randomizer.dll /IMPLIB:build\legacy_randomizer.lib build\spike_eswc.obj user32.lib kernel32.lib || exit /b 1
echo built dist\dinput8.dll and dist\legacy_randomizer.dll
exit /b 0
:novc
echo MSVC vcvarsamd64_x86.bat not found
exit /b 1
