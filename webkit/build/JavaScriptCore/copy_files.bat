@echo off

set DIR=%1
set JAVASCRIPTCORE_DIR="..\..\..\third_party\WebKit\JavaScriptCore"
setlocal

mkDIR 2>NUL %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\APICast.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JavaScript.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JSBase.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JSContextRef.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JSObjectRef.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JSStringRef.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JSStringRefCF.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JSStringRefBSTR.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JSValueRef.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JavaScriptCore.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\JSRetainPtr.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\OpaqueJSString.h" %DIR%
xcopy /y /d "%JAVASCRIPTCORE_DIR%\API\WebKitAvailability.h" %DIR%

endlocal
