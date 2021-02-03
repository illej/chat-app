@echo off

echo Downloading enet-1.3.15

set url=http://enet.bespin.org/download/enet-1.3.15.tar.gz
set file=enet.tar.gz
set opts=--show-error --progress-bar

curl %opts% %url% -o %file%

set err=%ERRORLEVEL%
if %err% neq 0 goto bad

echo Extracting
tar -xzf %file%

set err=%ERRORLEVEL%
if %err% neq 0 goto bad

echo Copying libs
if not exist external mkdir external
pushd external
if not exist lib mkdir lib
if not exist include mkdir include
popd

robocopy .\enet-1.3.15 .\external\lib enet64.lib /NJH /NJS /NDL

echo Copying headers
robocopy .\enet-1.3.15\include\ .\external\include\ /E /NJH /NJS /NDL

echo Cleaning up
rmdir /Q /S enet-1.3.15
del %file%

echo Building app
call build.bat

goto end

:bad
	echo.
    echo --! Setup failed (%err%)
	goto end

:end
	echo Done
