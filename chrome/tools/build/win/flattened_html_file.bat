:: Batch file run as build command for flattening files
:: The custom build rule is set to expect (inputfile).html
@echo off

setlocal

set InFile=%~1
set SolutionDir=%~2
set OutFile=%~3

:: Put cygwin in the path
call %SolutionDir%\..\third_party\cygwin\setup_env.bat

%SolutionDir%\..\third_party\python_24\python.exe %SolutionDir%\tools\build\win\html_inline.py %InFile% %OutFile%
