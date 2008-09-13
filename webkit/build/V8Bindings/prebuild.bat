@echo off

setlocal
set OUTDIR=%1
set INTDIR=%2
set CYGWIN_ROOT=%~dp0..\..\..\third_party\cygwin\
set GNU_ROOT=%~dp0..\..\..\third_party\gnu\files

set PATH=%CYGWIN_ROOT%bin;%GNU_ROOT%;%SystemRoot%;%SystemRoot%\system32

:: Ensure that the cygwin mount points are defined
CALL %CYGWIN_ROOT%setup_mount.bat > NUL

:: Fix file access on vista: without this flag, the files may not be accessible.
set CYGWIN=nontsec

:: Copy files that the V8 bindings share with the KJS bindings.
call copy_files.bat %IntDir%\SharedSources

:: Build the generated files from IDL files.
bash build-generated-files.sh ..\..\..\third_party\WebKit\WebCore ..\..\port "%OUTDIR%" "%INTDIR%"
