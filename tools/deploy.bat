@echo off

setlocal
setlocal EnableDelayedExpansion

set Config=Debug
set Suffix=x64
set Deploy=C:\Deploy\winspd
set Target=Win10DBG
set Chkpnt=winspd
if not X%1==X set Target=%1
if not X%2==X set Chkpnt=%2

(
    echo certmgr /add /c winspd-%SUFFIX%.cer /s /r localMachine root
    echo certmgr /add /c winspd-%SUFFIX%.cer /s /r localMachine TrustedPublisher
    echo devsetup-x64 add root\winspd winspd-%SUFFIX%.inf
    echo sc create WinSpd.Launcher binPath=%%~dp0launcher-%SUFFIX%.exe
    echo sc start WinSpd.Launcher
    echo reg add HKLM\Software\WinSpd\Services\rawdisk /v Executable /d %%~dp0rawdisk-%SUFFIX%.exe /reg:32 /f
    echo reg add HKLM\Software\WinSpd\Services\rawdisk /v CommandLine /d "-f %%%%1" /reg:32 /f
    echo reg add HKLM\Software\WinSpd\Services\rawdisk /v Security /d "D:P(A;;RP;;;WD)" /reg:32 /f
    echo reg add HKCR\.rawdisk /ve /d WinSpd.DiskFile /f
    echo reg add HKCR\.rawdisk\ShellNew /v NullFile /f
    echo regsvr32 /s %%~dp0shellex-%SUFFIX%.dll
) >%~dp0..\build\VStudio\build\%Config%\deploy-setup.bat

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot="%%j"
)
set CERTMGR_DIR=
for /r %KitRoot%bin %%f in (%SUFFIX%\certmgr.ex?) do (
    set CERTMGR_DIR="%%~dpf"
)

set Files=
for %%f in (
    %~dp0..\build\VStudio\build\%Config%\
        winspd-%SUFFIX%.inf
        winspd-%SUFFIX%.cat
        winspd-%SUFFIX%.cer
        winspd-%SUFFIX%.sys
        winspd-%SUFFIX%.dll
        devsetup-%SUFFIX%.exe
        launcher-%SUFFIX%.exe
        launchctl-%SUFFIX%.exe
        shellex-%SUFFIX%.dll
        scsitool-%SUFFIX%.exe
        stgtest-%SUFFIX%.exe
        rawdisk-%SUFFIX%.exe
        winspd-tests-%SUFFIX%.exe
        deploy-setup.bat
    %~dp0..\tools\
        scsicompliance.bat
    !CERTMGR_DIR!
        certmgr.exe
    ) do (
    set File=%%~f
    if [!File:~-1!] == [\] (
        set Dir=!File!
    ) else (
        if not [!Files!] == [] set Files=!Files!,
        set Files=!Files!'!Dir!!File!'
    )
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0deploy.ps1' -Name '%Target%' -CheckpointName '%Chkpnt%' -Files !Files! -Destination '%Deploy%'"
