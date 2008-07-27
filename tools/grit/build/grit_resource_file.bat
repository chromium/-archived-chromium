:: Batch file run as build command for .grd files
:: The custom build rule is set to expect (inputfile).h and (inputfile).rc
:: our grd files must generate files with the same basename.
@echo off

setlocal

set InFile=%~1
set SolutionDir=%~2
set InputDir=%~3

:: Use GNU tools
call %SolutionDir%\..\third_party\gnu\setup_env.bat

%SolutionDir%\..\third_party\python_24\python.exe %SolutionDir%\..\tools\grit\grit.py -i %InFile% build -o %InputDir%
