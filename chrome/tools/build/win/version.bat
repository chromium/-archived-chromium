:: Batch file run as build command for vers.vcproj
@echo off

setlocal

set InFile=%~1
set SolutionDir=%~2
set IntDir=%~3
set OutFile=%~4
set VarsBat=%IntDir%/vers-vars.bat

:: Put cygwin in the path
call %SolutionDir%\..\third_party\cygwin\setup_env.bat

:: Load version digits as environment variables
cat %SolutionDir%\VERSION | sed "s/\(.*\)/set \1/" > %VarsBat%

:: Load branding strings as environment variables
set Distribution="chromium"
if "%CHROMIUM_BUILD%" == "_google_chrome" set Distribution="google_chrome"
cat %SolutionDir%app\theme\%Distribution%\BRANDING | sed "s/\(.*\)/set \1/" >> %VarsBat%

set OFFICIAL_BUILD=0
if "%CHROME_BUILD_TYPE%" == "_official" set OFFICIAL_BUILD=1

:: Determine the current repository revision number
set PATH=%~dp0..\..\..\..\third_party\svn;%PATH%
svn.exe info | grep.exe "Revision:" | cut -d" " -f2- | sed "s/\(.*\)/set LASTCHANGE=\1/" >> %VarsBat%
call %VarsBat%

::echo LastChange: %LASTCHANGE%

:: output file
cat %InFile% | sed "s/@MAJOR@/%MAJOR%/" ^
                  | sed "s/@MINOR@/%MINOR%/" ^
                  | sed "s/@BUILD@/%BUILD%/" ^
                  | sed "s/@PATCH@/%PATCH%/" ^
                  | sed "s/@COMPANY_FULLNAME@/%COMPANY_FULLNAME%/" ^
                  | sed "s/@COMPANY_SHORTNAME@/%COMPANY_SHORTNAME%/" ^
                  | sed "s/@PRODUCT_FULLNAME@/%PRODUCT_FULLNAME%/" ^
                  | sed "s/@PRODUCT_SHORTNAME@/%PRODUCT_SHORTNAME%/" ^
                  | sed "s/@PRODUCT_EXE@/%PRODUCT_EXE%/" ^
                  | sed "s/@COPYRIGHT@/%COPYRIGHT%/" ^
                  | sed "s/@OFFICIAL_BUILD@/%OFFICIAL_BUILD%/" ^
                  | sed "s/@LASTCHANGE@/%LASTCHANGE%/" > %OutFile%

endlocal
