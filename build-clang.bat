@echo off
call "%VCINSTALLDIR%\Auxiliary\Build\vcvars32.bat" || exit /b 1
cmake --preset clang -DCMAKE_BUILD_TYPE=RelWithDebInfo || exit /b 1
cmake --build build-clang || exit /b 1
