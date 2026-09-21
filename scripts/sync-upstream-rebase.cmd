@echo off
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0sync-upstream-rebase.ps1" %*
exit /b %ERRORLEVEL%
