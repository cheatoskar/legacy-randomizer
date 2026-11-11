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
rem Horizon plugin dist\legacy_randomizer_plugin.dll and the UI preview build\test\ui_preview.exe (Dear ImGui 1.91.5, third_party\imgui)
if not exist build\imgui mkdir build\imgui
if not exist build\plugin mkdir build\plugin
cl /nologo /c /O2 /MT /GS /W4 /WX /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /I third_party\imgui /Fo"build\imgui\\" third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp third_party\imgui\backends\imgui_impl_dx9.cpp third_party\imgui\backends\imgui_impl_win32.cpp || exit /b 1
cl /nologo /c /O2 /MT /GS /W4 /WX /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /I src /I third_party\imgui /Fo"build\plugin\\" src\plugin\fetcher.cpp src\plugin\history.cpp src\plugin\host_horizon.cpp src\plugin\image.cpp src\plugin\plugin.cpp src\plugin\run.cpp src\plugin\settings.cpp src\plugin\tmtext.cpp src\plugin\ui.cpp src\plugin\util.cpp src\core\tmx.cpp src\core\session.cpp src\core\gbx.cpp || exit /b 1
set "LR_OBJ=build\plugin\fetcher.obj build\plugin\history.obj build\plugin\image.obj build\plugin\plugin.obj build\plugin\run.obj build\plugin\settings.obj build\plugin\tmtext.obj build\plugin\ui.obj build\plugin\util.obj build\plugin\tmx.obj build\plugin\session.obj build\plugin\gbx.obj"
set "IMGUI_OBJ=build\imgui\imgui.obj build\imgui\imgui_draw.obj build\imgui\imgui_tables.obj build\imgui\imgui_widgets.obj"
link /nologo /DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /OUT:dist\legacy_randomizer_plugin.dll /IMPLIB:build\legacy_randomizer_plugin.lib build\plugin\host_horizon.obj %LR_OBJ% %IMGUI_OBJ% wininet.lib ole32.lib windowscodecs.lib user32.lib kernel32.lib || exit /b 1
cl /nologo /c /O2 /MT /GS /W4 /WX /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /I src /I third_party\imgui /Fo"build\test\\" test\ui_preview.cpp || exit /b 1
link /nologo /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT /SUBSYSTEM:CONSOLE /OUT:build\test\ui_preview.exe build\test\ui_preview.obj %LR_OBJ% %IMGUI_OBJ% build\imgui\imgui_impl_dx9.obj build\imgui\imgui_impl_win32.obj d3d9.lib wininet.lib ole32.lib windowscodecs.lib user32.lib gdi32.lib kernel32.lib || exit /b 1
echo built dist\dinput8.dll and dist\legacy_randomizer.dll and build\test\core_test.exe
echo built dist\legacy_randomizer_plugin.dll and build\test\ui_preview.exe
exit /b 0
:novc
echo MSVC vcvarsamd64_x86.bat not found
exit /b 1
