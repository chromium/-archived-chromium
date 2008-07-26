:: A wrapper file for running create_string_rc.py from visual studio.

setlocal
set OUTFILE=%~1
set PYTHON=%~dp0..\..\..\..\third_party\python_24\python.exe

:: Add grit to the python path so we can import FP.py.
set PYTHONPATH=%~dp0..\..\..\..\tools\grit\grit\extern

%PYTHON% create_string_rc.py %OUTFILE%
