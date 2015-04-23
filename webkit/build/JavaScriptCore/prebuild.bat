REM @echo off

setlocal
set IntDir=%1
set OutDir=%2
set CYGWIN_ROOT=%~dp0..\..\..\third_party\cygwin\
set GNU_ROOT=%~dp0..\..\..\third_party\gnu\files

set PATH=%CYGWIN_ROOT%bin;%GNU_ROOT%;%SystemRoot%;%SystemRoot%\system32

:: Ensure that the cygwin mount points are defined
CALL %CYGWIN_ROOT%setup_mount.bat > NUL

:: Fix tempfile() on vista: without this flag, the files that it creates are not accessible.
set CYGWIN=nontsec

:: Help dftables script to find a usable temporary directory. For an unknown
:: reason, /tmp is not defined on cygwin and this script looks at TMPDIR
:: environment variable, which is usually not defined on Windows either.
if "%TMPDIR%" == "" (
	REM It fails in even stranger way if the TEMP folder is not owned by the
	REM current user, so create a folder to make sure we are the owner.
	set TMPDIR=%TEMP%\javascriptcore_pcre
	mkdir %TEMP%\javascriptcore_pcre
	bash build-generated-files.sh "%IntDir%" "..\..\..\third_party\WebKit"
	rd /q /s %TEMP%\javascriptcore_pcre
) else (
	bash build-generated-files.sh "%IntDir%" "..\..\..\third_party\WebKit"
)

call copy_files.bat %IntDir%\JavaScriptCore
endlocal
