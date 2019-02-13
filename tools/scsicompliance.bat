@echo off

setlocal
setlocal EnableDelayedExpansion

set Arch=amd64

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1
if !ERRORLEVEL! equ 0 (
    for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
        set KitRoot=%%jHardware Lab Kit\
    )
)

if not exist "%KitRoot%" set "KitRoot=C:\Program Files (x86)\Windows Kits\8.1\Hardware Certification Kit\"

set PATH=%KitRoot%..\Testing\Runtimes\TAEF;%PATH%
"%KitRoot%Tests\%Arch%\nttest\driverstest\storage\wdk\scsicompliance.exe" %*
