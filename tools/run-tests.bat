@echo off

setlocal
setlocal EnableDelayedExpansion

set Configuration=Release
if not X%1==X set Configuration=%1

cd %~dp0..
set ProjRoot=%cd%

cd build\VStudio
if not exist build\%Configuration% echo === No tests found >&2 & goto fail
cd build\%Configuration%

set dfl_tests=^
    rawdisk-cc-stgtest-pipe-x64 ^
    rawdisk-cc-stgtest-pipe-x86 ^
    rawdisk-cc-stgtest-pipe-msil ^
    rawdisk-nc-stgtest-pipe-x64 ^
    rawdisk-nc-stgtest-pipe-x86 ^
    rawdisk-nc-stgtest-pipe-msil ^
    rawdisk-cc-stgtest-raw-x64 ^
    rawdisk-cc-stgtest-raw-x86 ^
    rawdisk-cc-stgtest-raw-msil ^
    rawdisk-nc-stgtest-raw-x64 ^
    rawdisk-nc-stgtest-raw-x86 ^
    rawdisk-nc-stgtest-raw-msil ^
    rawdisk-nu-stgtest-raw-x64 ^
    rawdisk-nu-stgtest-raw-x86 ^
    rawdisk-nu-stgtest-raw-msil ^
    rawdisk-cc-format-ntfs-x64 ^
    rawdisk-cc-format-ntfs-x86 ^
    rawdisk-cc-format-ntfs-msil ^
    rawdisk-nc-format-ntfs-x64 ^
    rawdisk-nc-format-ntfs-x86 ^
    rawdisk-nc-format-ntfs-msil ^
    rawdisk-nu-format-ntfs-x64 ^
    rawdisk-nu-format-ntfs-x86 ^
    rawdisk-nu-format-ntfs-msil ^
    rawdisk-scsicompliance-x64 ^
    rawdisk-scsicompliance-x86 ^
    rawdisk-scsicompliance-msil
set opt_tests=^
    winspd-tests-x64 ^
    winspd-tests-x86

set tests=
for %%f in (%dfl_tests%) do (
    if X%2==X (
        set tests=!tests! %%f
    ) else (
        set test=%%f
        if not "X!test:%2=!"=="X!test!" set tests=!tests! %%f
    )
)
for %%f in (%opt_tests%) do (
    if X%2==X (
        rem
    ) else (
        set test=%%f
        if not "X!test:%2=!"=="X!test!" set tests=!tests! %%f
    )
)

set testpass=0
set testfail=0
for %%f in (%tests%) do (
    echo === Running %%f

    if defined APPVEYOR (
        appveyor AddTest "%%f" -FileName None -Framework None -Outcome Running
    )

    pushd %cd%
    call :%%f
    popd

    if !ERRORLEVEL! neq 0 (
        set /a testfail=testfail+1

        echo === Failed %%f

        if defined APPVEYOR (
            appveyor UpdateTest "%%f" -FileName None -Framework None -Outcome Failed -Duration 0
        )
    ) else (
        set /a testpass=testpass+1

        echo === Passed %%f

        if defined APPVEYOR (
            appveyor UpdateTest "%%f" -FileName None -Framework None -Outcome Passed -Duration 0
        )
    )
    echo:
)

set /a total=testpass+testfail
echo === Total: %testpass%/%total%
call :leak-test
if !ERRORLEVEL! neq 0 goto fail
if not %testfail%==0 goto fail

exit /b 0

:fail
exit /b 1

:winspd-tests-x64
winspd-tests-x64 *
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winspd-tests-x86
winspd-tests-x86 *
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-stgtest-pipe-common
set TestExit=0
start "" /b rawdisk-%1 -p \\.\pipe\rawdisk -f test.disk %~3
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
stgtest-x64 \\.\pipe\rawdisk\0 %2 WRUR * *
if !ERRORLEVEL! neq 0 set TestExit=1
taskkill /f /im rawdisk-%1.exe
del test.disk 2>nul
exit /b !TestExit!

:rawdisk-cc-stgtest-pipe-x64
call :rawdisk-stgtest-pipe-common x64 10000 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-cc-stgtest-pipe-x86
call :rawdisk-stgtest-pipe-common x86 10000 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-cc-stgtest-pipe-msil
call :rawdisk-stgtest-pipe-common dotnet-msil 10000 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-stgtest-pipe-x64
call :rawdisk-stgtest-pipe-common x64 1000 "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-stgtest-pipe-x86
call :rawdisk-stgtest-pipe-common x86 1000 "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-stgtest-pipe-msil
call :rawdisk-stgtest-pipe-common dotnet-msil 1000 "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-stgtest-raw-common
set TestExit=0
start "" /b rawdisk-%1 -f test.disk %~3
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
call :diskpart-partition 1 R
stgtest-x64 \\.\R: %2 WR * *
if !ERRORLEVEL! neq 0 set TestExit=1
call :diskpart-remove 1 R
taskkill /f /im rawdisk-%1.exe
del test.disk 2>nul
exit /b !TestExit!

:rawdisk-cc-stgtest-raw-x64
call :rawdisk-stgtest-raw-common x64 10000 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-cc-stgtest-raw-x86
call :rawdisk-stgtest-raw-common x86 10000 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-cc-stgtest-raw-msil
call :rawdisk-stgtest-raw-common dotnet-msil 10000 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-stgtest-raw-x64
call :rawdisk-stgtest-raw-common x64 1000 "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-stgtest-raw-x86
call :rawdisk-stgtest-raw-common x86 1000 "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-stgtest-raw-msil
call :rawdisk-stgtest-raw-common dotnet-msil 1000 "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nu-stgtest-raw-x64
call :rawdisk-stgtest-raw-common x64 1000 "-C 1 -U 0"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nu-stgtest-raw-x86
call :rawdisk-stgtest-raw-common x86 1000 "-C 1 -U 0"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nu-stgtest-raw-msil
call :rawdisk-stgtest-raw-common dotnet-msil 1000 "-C 1 -U 0"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-format-ntfs-common
set TestExit=0
start "" /b rawdisk-%1 -f test.disk %~2
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
call :diskpart-format-ntfs 1 R
pushd >nul
cd R: >nul 2>nul
if !ERRORLEVEL! neq 0 set TestExit=1
if X!TestExit!==X0 (
    R:
    echo hello>world
    if !ERRORLEVEL! neq 0 set TestExit=1
    type world
    if !ERRORLEVEL! neq 0 set TestExit=1
    dir
    if !ERRORLEVEL! neq 0 set TestExit=1
)
popd
call :diskpart-remove 1 R
taskkill /f /im rawdisk-%1.exe
del test.disk 2>nul
exit /b !TestExit!

:rawdisk-cc-format-ntfs-x64
call :rawdisk-format-ntfs-common x64 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-cc-format-ntfs-x86
call :rawdisk-format-ntfs-common x86 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-cc-format-ntfs-msil
call :rawdisk-format-ntfs-common dotnet-msil "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-format-ntfs-x64
call :rawdisk-format-ntfs-common x64 "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-format-ntfs-x86
call :rawdisk-format-ntfs-common x86 "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nc-format-ntfs-msil
call :rawdisk-format-ntfs-common dotnet-msil "-C 0 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nu-format-ntfs-x64
call :rawdisk-format-ntfs-common x64 "-C 1 -U 0"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nu-format-ntfs-x86
call :rawdisk-format-ntfs-common x86 "-C 1 -U 0"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-nu-format-ntfs-msil
call :rawdisk-format-ntfs-common dotnet-msil "-C 1 -U 0"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-scsicompliance-common
set TestExit=0
start "" /b rawdisk-%1 -f test.disk %~3
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
call :scsicompliance /device \\.\PhysicalDrive1 /operation test /scenario common /verbosity 2
if !ERRORLEVEL! neq 0 set TestExit=1
taskkill /f /im rawdisk-%1.exe
del test.disk 2>nul
exit /b !TestExit!

:rawdisk-scsicompliance-x64
call :rawdisk-scsicompliance-common x64 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-scsicompliance-x86
call :rawdisk-scsicompliance-common x86 "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:rawdisk-scsicompliance-msil
call :rawdisk-scsicompliance-common dotnet-msil "-C 1 -U 1"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:diskpart-partition
echo rescan                             > %TMP%\diskpart.script
echo select disk %1                     >>%TMP%\diskpart.script
echo attribute disk clear readonly noerr>>%TMP%\diskpart.script
echo online disk noerr                  >>%TMP%\diskpart.script
echo clean                              >>%TMP%\diskpart.script
echo create partition primary           >>%TMP%\diskpart.script
echo assign letter %2                   >>%TMP%\diskpart.script
echo exit                               >>%TMP%\diskpart.script
diskpart /s %TMP%\diskpart.script
del %TMP%\diskpart.script 2>nul
exit /b 0

:diskpart-format-ntfs
echo rescan                             > %TMP%\diskpart.script
echo select disk %1                     >>%TMP%\diskpart.script
echo attribute disk clear readonly noerr>>%TMP%\diskpart.script
echo online disk noerr                  >>%TMP%\diskpart.script
echo clean                              >>%TMP%\diskpart.script
echo create partition primary           >>%TMP%\diskpart.script
echo format fs=ntfs quick               >>%TMP%\diskpart.script
echo assign letter %2                   >>%TMP%\diskpart.script
echo exit                               >>%TMP%\diskpart.script
diskpart /s %TMP%\diskpart.script
del %TMP%\diskpart.script 2>nul
exit /b 0

:diskpart-remove
echo rescan                             > %TMP%\diskpart.script
echo select disk %1                     >>%TMP%\diskpart.script
echo select partition 1                 >>%TMP%\diskpart.script
echo remove letter %2                   >>%TMP%\diskpart.script
echo exit                               >>%TMP%\diskpart.script
diskpart /s %TMP%\diskpart.script
del %TMP%\diskpart.script 2>nul
exit /b 0

:scsicompliance
set ScsiComplianceTestFound=
set ScsiComplianceTestName=
set ScsiComplianceTestActive=
set ScsiComplianceTestLines=
set ScsiComplianceTestStatus=
set ScsiComplianceTestExit=0
(SET LF=^
%=this line is empty=%
)
for /F "delims=" %%l in ('call "%ProjRoot%\tools\scsicompliance.bat" %* ^| findstr /n "^"') do (
    set ScsiComplianceTestLine=%%l
    set ScsiComplianceTestLine=!ScsiComplianceTestLine:*:=!

    for /F "tokens=1,2,* delims=:" %%h in ("%%l") do (
        set FieldName=%%i
        set FieldName=!FieldName: =!
        set FieldValue=%%j

        if X!FieldName!==XStart (
            for /F "tokens=1 delims=," %%a in ("!FieldValue!") do (
                set ScsiComplianceTestName=####%%a
                set ScsiComplianceTestName=!ScsiComplianceTestName:#### =####!
                set ScsiComplianceTestName=!ScsiComplianceTestName:####=!
                set ScsiComplianceTestName=!ScsiComplianceTestName:ASSERTION: =!
            )
            set ScsiComplianceTestActive=YES
            set ScsiComplianceTestLines=
            set ScsiComplianceTestFound=YES
        )

        if X!ScsiComplianceTestActive!==XYES (
            set ScsiComplianceTestLines=!ScsiComplianceTestLines!!LF!    !ScsiComplianceTestLine!
        )

        if X!FieldName!==XEnd (
            for /F "tokens=1 delims=," %%a in ("!FieldValue!") do (
                set ScsiComplianceTestStatus=####%%a
                set ScsiComplianceTestStatus=!ScsiComplianceTestStatus:#### =####!
                set ScsiComplianceTestStatus=!ScsiComplianceTestStatus:####=!
            )

            set ScsiComplianceTestPrefix=!ScsiComplianceTestName!..............................................................
            set ScsiComplianceTestPrefix=!ScsiComplianceTestPrefix:~0,63!
            if X!ScsiComplianceTestStatus!==XPass (
                echo !ScsiComplianceTestPrefix! OK
            ) else if X!ScsiComplianceTestStatus!==XWarn (
                echo !ScsiComplianceTestPrefix! SKIP
            ) else (
                echo !ScsiComplianceTestPrefix! KO!ScsiComplianceTestLines!
                set ScsiComplianceTestExit=1
            )

            set ScsiComplianceTestName=
            set ScsiComplianceTestActive=
            set ScsiComplianceTestLines=
            set ScsiComplianceTestStatus=
        )
    )
)
if not X!ScsiComplianceTestFound!==XYES set ScsiComplianceTestExit=1
exit /b !ScsiComplianceTestExit!

:leak-test
for /F "tokens=1,2 delims=:" %%i in ('verifier /query ^| findstr ^
    /c:"Current Pool Allocations:" ^
    /c:"CurrentPagedPoolAllocations:" ^
    /c:"CurrentNonPagedPoolAllocations:"'
    ) do (

    set FieldName=%%i
    set FieldName=!FieldName: =!

    set FieldValue=%%j
    set FieldValue=!FieldValue: =!
    set FieldValue=!FieldValue:^(=!
    set FieldValue=!FieldValue:^)=!

    if X!FieldName!==XCurrentPoolAllocations (
        for /F "tokens=1,2 delims=/" %%k in ("!FieldValue!") do (
            set NonPagedAlloc=%%k
            set PagedAlloc=%%l
        )
    ) else if X!FieldName!==XCurrentPagedPoolAllocations (
        set PagedAlloc=!FieldValue!
    ) else if X!FieldName!==XCurrentNonPagedPoolAllocations (
        set NonPagedAlloc=!FieldValue!
    )
)
set /A TotalAlloc=PagedAlloc+NonPagedAlloc
if !TotalAlloc! equ 0 (
    echo === Leaks: None
) else (
    echo === Leaks: !NonPagedAlloc! NP / !PagedAlloc! P
    goto fail
)
exit /b 0
