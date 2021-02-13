@echo off

setlocal enabledelayedexpansion

:: This is necessary so that we can use "exit"
:: to terminate the batch file, and all
:: subroutines, but not the original cmd.exe
if "%selfWrapped%"=="" (
    set selfWrapped=true
    %ComSpec% /s /c ""%~0" %*"
    goto :eof
)

set arg1=%1
set enet=enet-1.3.15
set enet_tar=%enet%.tar.gz
set enet_dll=enet64.dll
set enet_lib=enet64.lib
set crash_file=.enet_build_failure

set need=false
call :check_enet_build need

if "%need%"=="true" (
    if not exist %enet% call :dl
    call :enet_build
)

echo Building chat-app

set ext_include_dir=external/include
set ext_lib_dir=external/lib

set compiler_flags=-nologo -I %ext_include_dir%
set link_flags=-link /LIBPATH:%ext_lib_dir%
:: set libs=ws2_32.lib winmm.lib enet64.lib
set libs=%enet_lib%

cl %compiler_flags% -Fe: server.exe src\chat_server.c  %link_flags% %libs% || goto :bad
cl %compiler_flags% -Fe: client.exe src\chat_client.c  %link_flags% %libs% || goto :bad

echo Build finished
goto :end

::
::functions
::
:check_enet_build
    if not exist %enet_dll% set %1=true
    if "%arg1%"=="-f" set %1=true
    if exist %crash_file% set %1=true

    set modified=false
    call :was_enet_modified modified
    if "%modified%"=="true" set %1=true

    goto :eof

:was_enet_modified
    set cache_file=.last_modified_file

    if exist %cache_file% (
        set /p last_file_time=<%cache_file%
    )

    pushd %enet%

    set last_file=
    for /f "tokens=*" %%i in ('dir /b /od /a-d') do (
        set last_file=%%i
        set current_file_time=%%~ti
    )

    popd

    if "%current_file_time" neq "" (
        if !current_file_time! geq !last_file_time! (
            set last_file_time=!current_file_time!
            set %1=true
        )
    )

    if "%last_file_time%" neq "" (
        echo %last_file_time% > %cache_file%
    )

    goto :eof

:dl
    echo Downloading %enet%

    set url=http://enet.bespin.org/download/enet-1.3.15.tar.gz
    set opts=--show-error --progress-bar

    curl %opts% %url% -o %enet_tar% || call :bad

    echo Extracting files
    tar -xzf %enet_tar% || call :bad

    if not exist external mkdir external
    pushd external
    if not exist lib mkdir lib
    if not exist include mkdir include
    popd

    robocopy .\%enet%\include\ .\external\include\ /E > nul || call :bad

    goto :eof

:enet_build
    if not exist %enet% call :dl
    echo Building %enet%

    echo %time% > %crash_file%

    set enet_cflags=-W3 -wd4477 -wd4996 -nologo -DENET_DLL -DENET_DEBUG -I include /Fe: %enet_dll%
    set enet_lflags=/LD -link ws2_32.lib winmm.lib
    set enet_src=^
      callbacks.c^
      compress.c^
      host.c^
      list.c^
      packet.c^
      peer.c^
      protocol.c^
      unix.c^
      win32.c

    pushd %enet%
    cl %enet_cflags% %enet_src% %enet_lflags% || call :bad
    copy %enet_dll% ..\ || call :bad
    copy %enet_lib% ..\external\lib || call :bad
    popd

    del %crash_file%

    goto :eof

:bad
    echo.
    echo --! Build failed (%ERRORLEVEL%)
    exit 0

:end