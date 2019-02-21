@echo off

setlocal

set CONFIG=Debug
set SUFFIX=x64
set TARGET_MACHINE=WIN8DBG
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winspd\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%

cd %~dp0..
mkdir %TARGET% 2>nul
for %%f in (devsetup-%SUFFIX%.exe winspd-%SUFFIX%.inf winspd-%SUFFIX%.cat winspd-%SUFFIX%.cer winspd-%SUFFIX%.sys winspd-%SUFFIX%.dll scsitool-%SUFFIX%.exe stgtest-%SUFFIX%.exe rawdisk-%SUFFIX%.exe winspd-tests-%SUFFIX%.exe launcher-%SUFFIX%.exe launchctl-%SUFFIX%.exe shellex-%SUFFIX%.dll) do (
    copy build\VStudio\build\%CONFIG%\%%f %TARGET% >nul
)
for %%f in (scsicompliance.bat) do (
    copy tools\%%f %TARGET% >nul
)

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot="%%j"
)
copy %KitRoot%\Tools\%SUFFIX%\devcon.exe %TARGET% >nul
copy %KitRoot%\bin\%SUFFIX%\certmgr.exe %TARGET% >nul

echo certmgr /add /c winspd-%SUFFIX%.cer /s /r localMachine root            > %TARGET%kminst.bat
echo certmgr /add /c winspd-%SUFFIX%.cer /s /r localMachine TrustedPublisher>>%TARGET%kminst.bat
echo devcon install winspd-%SUFFIX%.inf root\winspd                         >>%TARGET%kminst.bat

echo sc create WinSpd.Launcher binPath=%%~dp0launcher-%SUFFIX%.exe          > %TARGET%uminst.bat
echo sc start WinSpd.Launcher                                               >>%TARGET%uminst.bat
echo reg add HKLM\Software\WinSpd\Services\rawdisk /v Executable /d %%~dp0rawdisk-%SUFFIX%.exe /reg:32 /f   >>%TARGET%uminst.bat
echo reg add HKLM\Software\WinSpd\Services\rawdisk /v CommandLine /d "-f %%%%1" /reg:32 /f                  >>%TARGET%uminst.bat
echo reg add HKLM\Software\WinSpd\Services\rawdisk /v Security /d "D:P(A;;RP;;;WD)" /reg:32 /f              >>%TARGET%uminst.bat
echo reg add HKCR\.rawdisk /ve /d WinSpd.DiskFile /f                        >>%TARGET%uminst.bat
echo reg add HKCR\.rawdisk\ShellNew /v NullFile /f                          >>%TARGET%uminst.bat
echo regsvr32 %%~dp0shellex-%SUFFIX%.dll                                    >>%TARGET%uminst.bat
