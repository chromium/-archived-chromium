# debugger_unittests.py
#
# Run chrome debugger front-end tests.
# see also chrome/test/debugger/test_protocol.js

import optparse
import os.path
import subprocess
import sys
import time

import google.path_utils
import google.process_utils

def RunTests(build_dir=None):
  '''This is just a simple wrapper for running the test through v8_shell.
  Since v8_shell always returns 0 whether the test passes or fails, buildbot
  looks at stdout to test for failure.
  '''
  script_dir = google.path_utils.ScriptDir()
  chrome_dir = google.path_utils.FindUpward(script_dir, "chrome")
  v8_dir = google.path_utils.FindUpward(script_dir, "v8")
  if build_dir:
    v8_shell = os.path.join(build_dir, "v8_shell.exe")
  else:
    v8_shell = os.path.join(chrome_dir, "Debug", "v8_shell.exe")
    # look for Debug version first
    if not os.path.isfile(v8_shell):
      v8_shell = os.path.join(chrome_dir, "Release", "v8_shell.exe")
  cmd = [v8_shell,
         "--allow-natives-syntax",
         "--expose-debug-as", "debugContext", # these two are together
         os.path.join(chrome_dir, "browser", "resources", "debugger_shell.js"),
         os.path.join(v8_dir, "tests", "mjsunit.js"),
         os.path.join(chrome_dir, "test", "debugger", "test_protocol.js")
        ]
  (retcode, output) = google.process_utils.RunCommandFull(cmd, 
                                                          collect_output=True)
  if "Success" in output:
    return 0
  else:
    return 1

if __name__ == "__main__":
  parser = optparse.OptionParser("usage: %prog [--build_dir=dir]")
  parser.add_option("", "--build_dir",
                    help="directory where v8_shell.exe was built")
  (options, args) = parser.parse_args()
  ret = RunTests(options.build_dir)
  sys.exit(ret)
