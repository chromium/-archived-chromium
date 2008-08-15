:: Batch file run to copy rlz binaries
@echo off

setlocal

set InputPath=%~1
set OutDir=%~2

REM Remove this line when we start setting CHROMIUM_BUILD option on buildbots
set CHROMIUM_BUILD=_google_chrome

if NOT "%CHROMIUM_BUILD%" == "_google_chrome" goto END

xcopy /R /C /Y %InputPath% %OutDir%

:END
