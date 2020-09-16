@echo off

setlocal
setlocal EnableDelayedExpansion

set MsiName="WinSpd - Windows Storage Proxy Driver"
set CrossCert="%~dp0DigiCert High Assurance EV Root CA.crt"
set Issuer="DigiCert"
set Subject="Navimatics LLC"

set Configuration=Release
set SignedPackage=

if not X%1==X set Configuration=%1
if not X%2==X set SignedPackage=%2

call "%~dp0vcvarsall.bat" x64

if not X%SignedPackage%==X (
    if not exist "%~dp0..\build\VStudio\build\%Configuration%\winspd-*.msi" (echo previous build not found >&2 & exit /b 1)
    if not exist "%SignedPackage%" (echo signed package not found >&2 & exit /b 1)
    del "%~dp0..\build\VStudio\build\%Configuration%\winspd-*.msi"
    if exist "%~dp0..\build\VStudio\build\%Configuration%\winspd.*.nupkg" del "%~dp0..\build\VStudio\build\%Configuration%\winspd.*.nupkg"
    for /R "%SignedPackage%" %%f in (*.inf *.sys *.dll *.cat) do (
        copy "%%f" "%~dp0..\build\VStudio\build\%Configuration%\sysinst" >nul
    )
)

cd %~dp0..\build\VStudio
set signfail=0

if X%SignedPackage%==X (
    if exist build\ for /R build\ %%d in (%Configuration%) do (
        if exist "%%d" rmdir /s/q "%%d"
    )

    devenv winspd.sln /build "%Configuration%|x64"
    if errorlevel 1 goto fail
    devenv winspd.sln /build "%Configuration%|x86"
    if errorlevel 1 goto fail

    devenv winspd.sln /build "Installer.%Configuration%|x86" /project sysinst
    if errorlevel 1 goto fail

    for %%f in (winspd-x64.sys winspd-x64.dll winspd-x86.sys winspd-x86.dll winspd-x64.cat winspd-x86.cat) do (
        signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha1 /t http://timestamp.digicert.com build\%Configuration%\sysinst\%%f
        if errorlevel 1 set /a signfail=signfail+1
        signtool sign /as /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 build\%Configuration%\sysinst\%%f
        if errorlevel 1 set /a signfail=signfail+1
    )

    pushd build\%Configuration%
    echo .OPTION EXPLICIT >driver.ddf
    echo .Set CabinetFileCountThreshold=0 >>driver.ddf
    echo .Set FolderFileCountThreshold=0 >>driver.ddf
    echo .Set FolderSizeThreshold=0 >>driver.ddf
    echo .Set MaxCabinetSize=0 >>driver.ddf
    echo .Set MaxDiskFileCount=0 >>driver.ddf
    echo .Set MaxDiskSize=0 >>driver.ddf
    echo .Set CompressionType=MSZIP >>driver.ddf
    echo .Set Cabinet=on >>driver.ddf
    echo .Set Compress=on >>driver.ddf
    echo .Set CabinetNameTemplate=driver.cab >>driver.ddf
    echo .Set DiskDirectory1=. >>driver.ddf
    echo .Set DestinationDir=winspd >>driver.ddf
    echo sysinst\winspd.inf >>driver.ddf
    echo sysinst\winspd-x64.sys >>driver.ddf
    echo sysinst\winspd-x64.dll >>driver.ddf
    echo sysinst\winspd-x86.sys >>driver.ddf
    echo sysinst\winspd-x86.dll >>driver.ddf
    makecab /F driver.ddf
    signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /t http://timestamp.digicert.com driver.cab
    if errorlevel 1 set /a signfail=signfail+1
    popd
)

if not X%SignedPackage%==X (
    REM Recreate and resign CAT files as ones from sysdev are only good for Win10.
    del build\%Configuration%\sysinst\*.cat
    call "%~dp0mkcat.bat" x64 build\%Configuration%\sysinst winspd.inf winspd-x64.sys winspd-x64.dll winspd-x86.dll
    call "%~dp0mkcat.bat" x86 build\%Configuration%\sysinst winspd.inf winspd-x86.sys winspd-x86.dll
    for %%f in (winspd-x64.cat winspd-x86.cat) do (
        signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha1 /t http://timestamp.digicert.com build\%Configuration%\sysinst\%%f
        if errorlevel 1 set /a signfail=signfail+1
        signtool sign /as /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 build\%Configuration%\sysinst\%%f
        if errorlevel 1 set /a signfail=signfail+1
    )
)

set WINSPD_BUILD_SYSINST_SUPPRESS=1
devenv winspd.sln /build "Installer.%Configuration%|x86"
if errorlevel 1 goto fail
set WINSPD_BUILD_SYSINST_SUPPRESS=

for %%f in (build\%Configuration%\winspd-*.msi) do (
    signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha1 /t http://timestamp.digicert.com /d %MsiName% %%f
    if errorlevel 1 set /a signfail=signfail+1
    REM signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 /d %MsiName% %%f
    REM if errorlevel 1 set /a signfail=signfail+1
)

if not %signfail%==0 echo SIGNING FAILED! The product has been successfully built, but not signed.

REM !!!: remove when we have choco support!
exit /b 0

where /q choco.exe
if %ERRORLEVEL% equ 0 (
    for %%f in (build\%Configuration%\winspd-*.msi) do set Version=%%~nf
    set Version=!Version:winspd-=!

    copy ..\choco\* build\%Configuration%
    copy ..\choco\LICENSE.TXT /B + ..\..\License.txt /B build\%Configuration%\LICENSE.txt /B
    certutil -hashfile build\%Configuration%\winspd-!Version!.msi SHA256 >>build\%Configuration%\VERIFICATION.txt
    choco pack build\%Configuration%\winspd.nuspec --version=!Version! --outputdirectory=build\%Configuration%
    if errorlevel 1 goto fail
)

exit /b 0

:fail
exit /b 1
