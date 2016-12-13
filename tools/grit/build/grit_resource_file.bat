:: Batch file run as build command for .grd files
:: The custom build rule is set to expect (inputfile).h and (inputfile).rc
:: our grd files must generate files with the same basename.
@echo off

setlocal

set InFile=%~1
set SolutionDir=%~2
set OutDir=%~3

:: We treat the next five args as preprocessor defines for grit.
set PreProc1=%~4
set PreProc2=%~5
set PreProc3=%~6
set PreProc4=%~7
set PreProc5=%~8

IF NOT EXIST %OutDir% (
mkdir %OutDir%
)

IF NOT (%PreProc1%)==() set PreProc1=-D %PreProc1%
IF NOT (%PreProc2%)==() set PreProc2=-D %PreProc2%
IF NOT (%PreProc3%)==() set PreProc3=-D %PreProc3%
IF NOT (%PreProc4%)==() set PreProc4=-D %PreProc4%
IF NOT (%PreProc5%)==() set PreProc5=-D %PreProc5%

:: Put cygwin in the path
call %SolutionDir%\..\third_party\cygwin\setup_env.bat

%SolutionDir%\..\third_party\python_24\python.exe %SolutionDir%\..\tools\grit\grit.py -i %InFile% build -o %OutDir% %PreProc1% %PreProc2% %PreProc3% %PreProc4% %PreProc5%
