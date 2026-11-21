@rem The sources come from an archive, not a clone, so stop git finding the repository the build folder sits in;
@rem outside a repository git apply patches the current folder.
for %%I in ("%CD%\..") do set "GIT_CEILING_DIRECTORIES=%%~fI"
echo Checking if patch is already applied...
git apply --reverse --check --ignore-whitespace %~dp0%1
if NOT ERRORLEVEL 1 echo Already applied, nothing to do && exit /b 0
echo Applying patch
git apply --ignore-whitespace %~dp0%1
