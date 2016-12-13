@echo off
:: %1 is source
:: %2 is output

:: Delete it if it existed
if exist "%2" del "%2"

:: Try to create a hardlink (argument are in reverse order). Hide errors if they occur, we have a fallback.
fsutil hardlink create "%2" "%1" > nul

:: If it failed, copy it instead. Don't hide errors if it fails.
if errorlevel 1 copy "%1" "%2"
