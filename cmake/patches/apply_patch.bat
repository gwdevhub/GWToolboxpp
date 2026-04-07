echo Checking if patch is already applied...
git apply --reverse --check %~dp0%1
if NOT ERRORLEVEL 1 echo Already applied, nothing to do && exit /b 0
echo Applying patch
git apply %~dp0%1
