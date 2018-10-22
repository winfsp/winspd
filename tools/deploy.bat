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
