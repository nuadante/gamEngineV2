@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "BASH_EXE="

rem PATH'te bash var mı?
for /f "delims=" %%i in ('where bash 2^>nul') do set "BASH_EXE=%%i"

rem Varsayılan kurulum yolu
if "%BASH_EXE%"=="" if exist "C:\Program Files\Git\bin\bash.exe" set "BASH_EXE=C:\Program Files\Git\bin\bash.exe"

if "%BASH_EXE%"=="" (
  echo Git Bash bulunamadi. Lutfen Git for Windows kurun: https://gitforwindows.org/
  exit /b 1
)

"%BASH_EXE%" "%SCRIPT_DIR%gitpush.sh" %*

endlocal

