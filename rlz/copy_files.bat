:: Batch file run to copy rlz binaries
@echo off

setlocal

set InputPath=%~1
set OutDir=%~2

if exist "%InputPath%" xcopy /R /C /Y "%InputPath%" "%OutDir%"
