@echo off
:: A wrapper file for running dependencies.py from visual studio.

setlocal
:: Dumpbin runs link.exe /dump which detects this environment variable and
:: ignores stdout. So we need to undefine it.
set VS_UNICODE_OUTPUT=
set PYTHON=%~dp0..\..\third_party\python_24\python.exe
%PYTHON% ..\tools\build\win\dependencies.py %*
