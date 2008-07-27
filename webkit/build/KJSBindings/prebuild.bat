@echo off

setlocal
set OUTDIR=%1
set INTDIR=%2
set CYGWIN_ROOT=%~dp0..\..\..\third_party\cygwin\
set GNU_ROOT=%~dp0..\..\..\third_party\gnu\files

set PATH=%CYGWIN_ROOT%bin;%GNU_ROOT%;%SystemRoot%;%SystemRoot%\system32

bash build-generated-files.sh ..\..\..\third_party\WebKit\WebCore ..\..\port "%OUTDIR%" "%INTDIR%"
