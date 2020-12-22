@echo off

setlocal
setlocal EnableDelayedExpansion

set vcvarsall="%~dp0vcvarsall.bat"

if "%1"=="x64" set OsVer=7_X64,8_X64,6_3_X64,10_X64,Server2008R2_X64,Server8_X64,Server6_3_X64,Server10_X64
if "%1"=="x86" set OsVer=7_X86,8_X86,6_3_X86,10_X86
if "%OsVer%"=="" goto usage
shift

if not "%1"=="-sign" goto skipsign
shift
set CertFile=%1
shift
if "%CertFile%"=="" goto usage
:skipsign

set BaseDir=%1
if "%BaseDir%"=="" goto usage

cd %BaseDir%
if errorlevel 1 goto fail

call %vcvarsall% x64
set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot="%%j"
)

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
%KitRoot%\bin\x86\inf2cat /driver:%TempDir% /os:%OsVer% /uselocaltime
if errorlevel 1 goto fail
if not "%CertFile%"=="" (
    for /F "delims=" %%l in ('certutil -dump "%CertFile%" ^| findstr /I /C:"Cert Hash(sha1)"') do (
        for /F "tokens=2 delims=:" %%h in ("%%l") do (
            set hash0=%%h
            set hash=!hash0: =!
        )
    )
    signtool sign /sha1 !hash! %TempDir%\*.cat
    if errorlevel 1 goto fail
)
copy %TempDir%\*.cat . >nul
if errorlevel 1 goto fail
rmdir /s/q %TempDir%
exit /b 0

:fail
rmdir /s/q %TempDir%
exit /b 1

:usage
echo usage: mkcat x86^|x64 [-sign certfile.cer] basedir files... 1>&2
exit /b 2
