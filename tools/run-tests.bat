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
    winspd-tests-x64 ^
    winspd-tests-x86 ^
    rawdisk-stgpipe-x64 ^
    rawdisk-stgpipe-x86 ^
    rawdisk-stgpipe-raw-x64 ^
    rawdisk-stgpipe-raw-x86
set opt_tests=

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

:rawdisk-stgpipe-x64
set TestExit=0
start "" /b rawdisk-x64 -p \\.\pipe\rawdisk -f test.disk
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
stgpipe-x64 \\.\pipe\rawdisk\0 10000 WRUR * *
if !ERRORLEVEL! neq 0 set TestExit=1
taskkill /f /im rawdisk-x64.exe
del test.disk 2>nul
exit /b !TestExit!

:rawdisk-stgpipe-x86
set TestExit=0
start "" /b rawdisk-x86 -p \\.\pipe\rawdisk -f test.disk
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
stgpipe-x86 \\.\pipe\rawdisk\0 10000 WRUR * *
if !ERRORLEVEL! neq 0 set TestExit=1
taskkill /f /im rawdisk-x86.exe
del test.disk 2>nul
exit /b !TestExit!

:rawdisk-stgpipe-raw-x64
set TestExit=0
start "" /b rawdisk-x64 -f test.disk
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
call :diskpart-partition 1 R
stgpipe-x64 \\.\R: 10000 WR * *
if !ERRORLEVEL! neq 0 set TestExit=1
call :diskpart-remove 1 R
taskkill /f /im rawdisk-x64.exe
del test.disk 2>nul
exit /b !TestExit!

:rawdisk-stgpipe-raw-x86
set TestExit=0
start "" /b rawdisk-x86 -f test.disk
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
call :diskpart-partition 1 R
stgpipe-x86 \\.\R: 10000 WR * *
if !ERRORLEVEL! neq 0 set TestExit=1
call :diskpart-remove 1 R
taskkill /f /im rawdisk-x86.exe
del test.disk 2>nul
exit /b !TestExit!

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

:diskpart-remove
echo rescan                             > %TMP%\diskpart.script
echo select disk %1                     >>%TMP%\diskpart.script
echo select partition 1                 >>%TMP%\diskpart.script
echo remove letter %2                   >>%TMP%\diskpart.script
echo exit                               >>%TMP%\diskpart.script
diskpart /s %TMP%\diskpart.script
del %TMP%\diskpart.script 2>nul
exit /b 0

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
