:: %1 is the input file js file
:: %2 is the solution directory

:: Change to the directory of the input file
cd %~p1

:: Make config.h
cscript //E:jscript %1 compiler=msvc
if errorlevel 1 echo %1: error: The javascript file failed to run.

:: Make sure the timestamp on config.h is current.
%2..\third_party\cygwin\bin\touch.exe ..\config.h

