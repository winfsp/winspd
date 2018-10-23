@echo off

setlocal

if "%1"=="x64" set OsVer=7_X64,8_X64,6_3_X64,10_X64,Server2008R2_X64,Server8_X64,Server6_3_X64,Server10_X64
if "%1"=="x86" set OsVer=7_X86,8_X86,6_3_X86,10_X86
if "%OsVer%"=="" goto usage
shift

set BaseDir=%1
if "%BaseDir%"=="" goto usage

cd %BaseDir%
if errorlevel 1 goto fail

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64

set TempDir=%TMP%\mkcat-%RANDOM%
if exist %TempDir% rmdir /s/q %TempDir%
mkdir %TempDir%
:copyloop
shift
if not "%1"=="" (
    copy %1 %TempDir% >nul
    if errorlevel 1 goto fail
    goto copyloop
)
echo inf2cat /driver:%TempDir% /os:%OsVer% /uselocaltime
inf2cat /driver:%TempDir% /os:%OsVer% /uselocaltime
if errorlevel 1 goto fail
copy %TempDir%\*.cat . >nul
if errorlevel 1 goto fail
rmdir /s/q %TempDir%
exit /b 0

:fail
rmdir /s/q %TempDir%
exit /b 1

:usage
echo usage: mkcat x86^|x64 basedir files... 1>&2
exit /b 2
