@echo off

setlocal
setlocal EnableDelayedExpansion

:varloop
if [%1]==[] goto vardone
if [%1]==[--] goto vardone
set var=%1
set var=!var::==!
call set !var!
shift
goto varloop
:vardone
shift

set src=%1
set dst=%2
if [!src!]==[] goto usage
if [!dst!]==[] goto usage

if exist !dst! del !dst!
for /f "delims=" %%l in (!src!) do (
    set line=%%l
    echo !line! >>!dst!
)

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot="%%j"
)
%KitRoot%\bin\x86\stampinf.exe -d * -v !MyVersion! -f !dst!
exit /b

:fail
exit /b 1

:usage
echo usage: mkinf variable:value... -- src dst 1>&2
exit /b 2
