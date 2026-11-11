@echo off
rem Builds the spike: dist\legacy_randomizer.dll and the test loader dist\dinput8.dll (x86, MSVC Build Tools).
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
if not exist "%VCVARS%" goto novc
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
if not exist dist mkdir dist
cl /nologo /c /O1 /GS- /W4 /WX /EHs-c- /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /Fo"build\\" src\testloader.cpp || exit /b 1
link /nologo /DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /NODEFAULTLIB /ENTRY:DllMain /DEF:src\testloader.def /OUT:dist\dinput8.dll /IMPLIB:build\dinput8.lib build\testloader.obj user32.lib kernel32.lib || exit /b 1
cl /nologo /c /O2 /MT /GS /W4 /WX /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /I src /Fo"build\\" src\spike.cpp src\reflect\reflect.cpp src\gamectl\gamectl.cpp src\core\gbx.cpp || exit /b 1
link /nologo /DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /OUT:dist\legacy_randomizer.dll /IMPLIB:build\legacy_randomizer.lib build\spike.obj build\reflect.obj build\gamectl.obj build\gbx.obj user32.lib kernel32.lib shell32.lib || exit /b 1
if not exist build\test mkdir build\test
cl /nologo /O2 /MT /GS /W4 /WX /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /Fo"build\test\\" /Fe"build\test\core_test.exe" test\core_test.cpp src\core\tmx.cpp src\core\session.cpp src\core\gbx.cpp wininet.lib user32.lib kernel32.lib /link /MACHINE:X86 || exit /b 1
echo built dist\dinput8.dll and dist\legacy_randomizer.dll and build\test\core_test.exe
exit /b 0
:novc
echo MSVC vcvarsamd64_x86.bat not found
exit /b 1
