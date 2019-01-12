@echo off

setlocal

set CONFIG=Debug
set SUFFIX=x64
set TARGET_MACHINE=WIN8DBG
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winspd\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%

cd %~dp0..
mkdir %TARGET% 2>nul
for %%f in (winspd-%SUFFIX%.inf winspd-%SUFFIX%.cat winspd-%SUFFIX%.cer winspd-%SUFFIX%.sys winspd-%SUFFIX%.dll scsitool-%SUFFIX%.exe stgtest-%SUFFIX%.exe rawdisk-%SUFFIX%.exe winspd-tests-%SUFFIX%.exe) do (
	copy build\VStudio\build\%CONFIG%\%%f %TARGET% >nul
)

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot="%%j"
)
copy %KitRoot%\Tools\%SUFFIX%\devcon.exe %TARGET% >nul
copy %KitRoot%\bin\%SUFFIX%\certmgr.exe %TARGET% >nul

echo certmgr /add /c winspd-%SUFFIX%.cer /s /r localMachine root >%TARGET%install.bat
echo certmgr /add /c winspd-%SUFFIX%.cer /s /r localMachine TrustedPublisher >>%TARGET%install.bat
echo devcon install winspd-%SUFFIX%.inf root\winspd >>%TARGET%install.bat
