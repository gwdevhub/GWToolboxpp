@echo off
rem build_launcher.bat: builds GWToolbox_Launcher_Windows, GWToolbox_Launcher_Linux for the relwithdebinfo preset. Extra arguments go to scripts\build.py.
setlocal
cd /d "%~dp0"
set "PYTHON="
for %%c in (python py) do if not defined PYTHON (%%c -c "import sys" >nul 2>&1 && set "PYTHON=%%c")
if not defined PYTHON (
    echo Python 3.9 or newer isn't on PATH. Run bootstrap.bat first.
    exit /b 1
)
"%PYTHON%" scripts\build.py --preset relwithdebinfo --target GWToolbox_Launcher_Windows --target GWToolbox_Launcher_Linux %*
exit /b %ERRORLEVEL%
