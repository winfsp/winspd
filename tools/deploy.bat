@echo off

setlocal

set CONFIG=Debug
set SUFFIX=x64
set TARGET_MACHINE=WIN8DBG
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winspd\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%

cd %~dp0..
mkdir %TARGET% 2>nul
for %%f in (winspd-%SUFFIX%.inf winspd-%SUFFIX%.cat winspd-%SUFFIX%.sys winspd-%SUFFIX%.dll) do (
	copy build\VStudio\build\%CONFIG%\%%f %TARGET% >nul
)

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot="%%j"
)
copy %KitRoot%\Tools\%SUFFIX%\devcon.exe %TARGET% >nul

echo devcon install winspd-%SUFFIX%.inf root\winspd >%TARGET%install.bat
