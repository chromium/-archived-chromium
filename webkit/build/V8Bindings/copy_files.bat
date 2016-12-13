@echo off

::
:: Some of the files used by the KJS bindings are not KJS specific and 
:: can therefore be used directly by the V8 bindings.  This script
:: copies those filed from the third_party KJS bindings directory
:: to the directory given as argument to the script.
::
set DIR=%1
set KJS_BINDINGS_DIR="..\..\..\third_party\WebKit\WebCore\bindings\js"
setlocal

mkdir 2>NUL %DIR%

endlocal

