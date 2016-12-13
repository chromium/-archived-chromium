@echo off
:: Copyright (c) 2009 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: Batch file run as build command for chrome_dll.vcproj

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

:: Look if subversion client is available. It may not be available on Windows
:: if downloaded with a tarball or depot_tools is not in the PATH.
call svn --version 2>nul 1>nul
:: If not available, just skip getting the revision number.
if errorlevel 1 goto :NO_SVN
goto :SET_ENV


:NO_SVN
:: Not having svn makes it impossible to determine the current checkout revision
:: number. On normal build, this is not an issue but for official builds, this
:: *can't* be tolerated so issue an error instead. VS will pick it up corectly.
set NO_SVN_LEVEL=error
if "%OFFICIAL_BUILD%" == "0" set NO_SVN_LEVEL=warning
echo %0(28) : %NO_SVN_LEVEL% : svn is not installed. Can't determine the revision number.
echo set LASTCHANGE=0 >> %VarsBat%
goto :GEN_FILE


:SET_ENV
call svn info | grep.exe "Revision:" | cut -d" " -f2- | sed "s/\(.*\)/set LASTCHANGE=\1/" >> %VarsBat%
goto :GEN_FILE


:GEN_FILE
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
