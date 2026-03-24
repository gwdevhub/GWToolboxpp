@echo off
setlocal

set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Debug

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build
set BIN_DIR=%SCRIPT_DIR%bin\%CONFIG%
set DLL=%BIN_DIR%\GWToolboxdll.dll
set LAUNCHER=%BIN_DIR%\GWToolbox.exe

echo Building (%CONFIG%)...
cmake --build "%BUILD_DIR%" --target GWToolboxdll GWToolbox --config %CONFIG% -j 2 || exit /b 1

echo Hot-reloading...
"%LAUNCHER%" /hotreload "%DLL%"
