@echo off

setlocal enabledelayedexpansion

set targetdir=enet-1.3.15
set file=last-change.log

:: get the last time from
:: the file
set /p last_file_time=<%file%

pushd %targetdir%

call :check

if "%current_file_time%" neq "" (
    if !current_file_time! geq !last_file_time! (
        set last_file_time=!current_file_time!
    )
)

popd

if "%last_file_time%" neq "" (
    echo %last_file_time% > %file%
)

goto :end

:check
    set last_file=

    for /f "tokens=*" %%a in ('dir /b /od /a-d') do (
        set last_file=%%a
        set current_file_time=%%~ta
    )

    echo %last_file% changed
    goto :eof

:end