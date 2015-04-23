#!/bin/sh

exec_dir=$(dirname $0)

if [ "$OSTYPE" = "cygwin" ]; then
  system_root=`cygpath "$SYSTEMROOT"`
  PATH="/usr/bin:$system_root/system32:$system_root:$system_root/system32/WBEM"
  export PATH
  python_prog="$exec_dir/../../../../third_party/python_24/python.exe"
else
  python_prog=python
  # When not using the included python, we don't get automatic site.py paths.
  # Specifically, run_webkit_tests needs the paths in:
  # third_party/python_24/Lib/site-packages/google.pth
  PYTHONPATH="${exec_dir}/../../../../tools/python:$PYTHONPATH"
  export PYTHONPATH
fi

"$python_prog" "$exec_dir/http_server.py" "$@"
