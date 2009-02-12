:: Batch file run as build command for .grd files
:: The custom build rule is set to expect (inputfile).h and (inputfile).rc
:: our grd files must generate files with the same basename.
@echo off

setlocal

set InFile=%~1
set SolutionDir=%~2
set OutDir=%~3

IF NOT EXIST %OutDir% (
mkdir %OutDir%
)

:: Put cygwin in the path
call %SolutionDir%\..\third_party\cygwin\setup_env.bat

%SolutionDir%\..\third_party\python_24\python.exe %SolutionDir%\..\tools\grit\grit.py -i %InFile% build -o %OutDir%
