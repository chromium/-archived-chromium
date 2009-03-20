#!/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

__doc__ = """
Wrapper script for executing pkg-config with the arguments supplied
on the command line and suppressing the exit status and error output
when the error is simply that the specified package isn't installed.
"""

import sys
import subprocess

p = subprocess.Popen(['pkg-config'] + sys.argv[1:],
                     stderr=subprocess.PIPE)
(stdout, stderr) = p.communicate()

exit_status = p.wait()

if exit_status == 1:
    import re
    if re.search('No package.*found', stderr):
        # Exit status of 1 with a presumably "normal" not found message.
        # Just swallow the "error."
        sys.exit(0)

sys.stderr.write(stderr)

sys.exit(exit_status)
