@echo off

setlocal

set CONFIG=Debug
set SUFFIX=x64
set TARGET_MACHINE=WIN8DBG
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winspd\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%
set DRIVER=winspd-%SUFFIX%.sys

cd %~dp0..
mkdir %TARGET% 2>nul
for %%f in (winspd-%SUFFIX%.sys winspd-%SUFFIX%.dll) do (
	copy build\VStudio\build\%CONFIG%\%%f %TARGET% >nul
)
echo sc create WinSpd type=kernel binPath=%%~dp0%DRIVER% >%TARGET%sc-create.bat
echo sc start WinSpd >%TARGET%sc-start.bat
echo sc stop WinSpd >%TARGET%sc-stop.bat
echo sc delete WinSpd >%TARGET%sc-delete.bat
