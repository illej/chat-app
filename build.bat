@echo off

set enet=enet-1.3.17
set enet_tar=%enet%.tar.gz
set enet_dll=enet64.dll
set enet_lib=enet64.lib

if not exist %enet% call :dl

echo Building %enet%
set enet_cflags=-WX -W3 -wd4477 -wd4996 -nologo -DENET_DLL -DENET_DEBUG -I include /Fe: %enet_dll%
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
call cl %enet_cflags% %enet_src% %enet_lflags% || call :bad
copy %enet_dll% ..\ || call :bad
copy %enet_lib% ..\external\lib || call :bad
popd
echo Build finished

echo Building chat-app
set ext_include_dir=external/include
set ext_lib_dir=external/lib
set compiler_flags=-nologo -I %ext_include_dir%
set link_flags=-link /LIBPATH:%ext_lib_dir%
set libs=%enet_lib%

call cl %compiler_flags% -Fe: server.exe src\chat_server.c %link_flags% %libs% || goto :bad
call cl %compiler_flags% -Fe: client.exe src\chat_client.c %link_flags% %libs% || goto :bad

echo Build finished
goto :end

:dl
    echo Downloading %enet%

    set url=http://enet.bespin.org/download/%enet_tar%
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

    echo Download finished
    goto :eof

:bad
    echo.
    echo -- Build failed (%ERRORLEVEL%)

:end
