@echo off

setlocal
setlocal EnableDelayedExpansion

set Configuration=Release

if not X%1==X set Configuration=%1

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64

cd %~dp0..\build\VStudio
set signfail=0

if exist build\ for /R build\ %%d in (%Configuration%) do (
    if exist "%%d" rmdir /s/q "%%d"
)

devenv winspd.sln /build "%Configuration%|x64"
if errorlevel 1 goto fail
devenv winspd.sln /build "%Configuration%|x86"
if errorlevel 1 goto fail

exit /b 0

:fail
exit /b 1
