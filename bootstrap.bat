@echo off
rem bootstrap.bat - checks and installs what Windows needs before scripts\bootstrap.py can run, then runs it.
rem bootstrap.py (shared with bootstrap.sh) installs llvm-mingw and vcpkg into the checkout, configures the build and
rem writes the CLion settings and GWToolbox.slnx for Visual Studio and Rider. No MSVC compiler is used.
rem
rem   1. Checks for git and Python 3.9+ (required), CMake 3.29+ and Ninja (installed with winget when missing), and
rem      Visual Studio with C++ (only for the .slnx). Nothing is changed until the required ones are all present.
rem   2. Runs scripts\bootstrap.py.
rem
rem Run it from the repo folder (or double-click it). Close CLion and Rider first: they rewrite their own .idea files
rem on exit and would overwrite the changes.

setlocal EnableExtensions DisableDelayedExpansion
set "REPO_ROOT=%~dp0"
set "REPO_ROOT=%REPO_ROOT:~0,-1%"
cd /d "%REPO_ROOT%"

for /f %%t in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%t"
if not exist "%REPO_ROOT%\logs" mkdir "%REPO_ROOT%\logs"
set "BOOTSTRAP_LOG=%REPO_ROOT%\logs\bootstrap-%STAMP%.log"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set PROBLEMS=0
set EXIT_CODE=0

call :say "bootstrap log: %BOOTSTRAP_LOG%"
call :say "==> checking prerequisites"

for %%i in (clion64.exe rider64.exe) do (
    tasklist /fi "imagename eq %%i" /nh 2>nul | find /i "%%i" >nul && call :problem "%%i is running. Close it and re-run, so it does not overwrite the .idea files."
)

where git >nul 2>&1 && (call :say "    git: found") || call :problem "git is not on PATH. Install it with: winget install --id Git.Git -e   Then open a new terminal and re-run."

set "PYTHON="
for %%c in (python py) do if not defined PYTHON call :find_python %%c
if defined PYTHON (call :say "    python: %PYTHON%") else call :problem "Python 3.9 or newer (scripts\require_python.py) is not on PATH. Install it with: winget install --id Python.Python.3.13 -e   Then open a new terminal and re-run."

set "HAS_WINGET="
where winget >nul 2>&1 && set HAS_WINGET=1

call :check_cmake
if defined CMAKE_OK (
    call :say "    cmake %CMAKE_VERSION%"
) else if defined HAS_WINGET (
    call :say "    cmake 3.29+: not found; it will be installed with winget"
) else (
    call :problem "CMake 3.29 or newer is missing and winget is not available. Install CMake from https://cmake.org/download/."
)

where ninja >nul 2>&1
if not errorlevel 1 (
    call :say "    ninja: found"
) else if defined HAS_WINGET (
    call :say "    ninja: not found; it will be installed with winget"
) else (
    call :problem "Ninja is missing and winget is not available. Install it from https://github.com/ninja-build/ninja/releases."
)

rem Only the .slnx needs Visual Studio, and only for its C++ project system: the compiler is llvm-mingw.
set "VS="
setlocal EnableDelayedExpansion
if exist "!VSWHERE!" for /f "delims=" %%v in ('call "!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CoreIde -property installationPath') do set "VS=%%v"
endlocal & set "VS=%VS%"
if defined VS (
    call :say "    Visual Studio with C++: %VS%"
) else (
    call :say "WARNING: Visual Studio with the Desktop development with C++ workload was not found; GWToolbox.slnx is still written, but needs it to open. CLion, Rider and the terminal do not."
)

if %PROBLEMS% gtr 0 (
    call :say "Missing prerequisites. Nothing was changed."
    set EXIT_CODE=1
    goto finish
)

if not defined CMAKE_OK (
    call :say "==> installing CMake with winget"
    winget install -e --id Kitware.CMake --accept-source-agreements --accept-package-agreements
    call :refresh_path
    call :check_cmake
)
if not defined CMAKE_OK (
    call :say "CMake was installed but this shell cannot see 3.29 or newer yet. Open a new terminal and re-run."
    set EXIT_CODE=1
    goto finish
)
where ninja >nul 2>&1
if errorlevel 1 (
    call :say "==> installing Ninja with winget"
    winget install -e --id Ninja-build.Ninja --accept-source-agreements --accept-package-agreements
    call :refresh_path
)
where ninja >nul 2>&1
if errorlevel 1 (
    call :say "Ninja was installed but this shell cannot see it yet. Open a new terminal and re-run."
    set EXIT_CODE=1
    goto finish
)

"%PYTHON%" "%REPO_ROOT%\scripts\bootstrap.py" --log "%BOOTSTRAP_LOG%"
set EXIT_CODE=%errorlevel%

rem Keeps a double-clicked window open to read; with input from nul (bootstrap.bat ^<nul) it does not wait.
:finish
echo(
if not "%EXIT_CODE%"=="0" echo The bootstrap failed; the messages above say why. The full log is %BOOTSTRAP_LOG%
echo Press any key to close this window...
pause >nul
exit /b %EXIT_CODE%


rem Echoed through delayed expansion, so cmd does not read the > in "==>" as a redirection.
:say
setlocal EnableDelayedExpansion
set "MESSAGE=%~1"
echo(!MESSAGE!
>>"%BOOTSTRAP_LOG%" echo(!MESSAGE!
endlocal
exit /b 0

:problem
set /a PROBLEMS+=1
call :say "  - %~1"
exit /b 0

rem Each python on PATH except the Microsoft Store placeholder, which only opens the Store; the scripts' own check
rem prints the interpreter's path when it is new enough.
:find_python
for /f "delims=" %%p in ('where %1 2^>nul') do (
    echo %%p| find /i "\WindowsApps\" >nul || (
        if not defined PYTHON for /f "delims=" %%e in ('call "%%p" "%REPO_ROOT%\scripts\require_python.py" 2^>nul') do set "PYTHON=%%e"
    )
)
exit /b 0

:check_cmake
set "CMAKE_OK="
set "CMAKE_VERSION="
for /f "tokens=3" %%v in ('cmake --version 2^>nul ^| findstr /b /c:"cmake version"') do set "CMAKE_VERSION=%%v"
if not defined CMAKE_VERSION exit /b 0
if not defined PYTHON exit /b 0
"%PYTHON%" -c "import sys; sys.exit(tuple(int(n) for n in sys.argv[1].split('.')[:2]) < (3, 29))" %CMAKE_VERSION% && set CMAKE_OK=1
exit /b 0

rem winget updates the machine and user PATH; pick that up without starting a new shell.
:refresh_path
for /f "delims=" %%p in ('powershell -NoProfile -Command "[Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')"') do set "PATH=%%p"
exit /b 0
