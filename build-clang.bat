@echo off
call "%VCINSTALLDIR%\Auxiliary\Build\vcvars32.bat" || exit /b 1
set "PATH=C:\Users\m\tools\clang+llvm-22.1.5-x86_64-pc-windows-msvc\bin;%PATH%"
cmake --preset clang -DCMAKE_BUILD_TYPE=RelWithDebInfo || exit /b 1
cmake --build build-clang || exit /b 1
