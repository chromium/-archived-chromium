#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Command for measuring how long pages take to load in a browser.

Prerequisites:
  1. The command_line package from tools/site_compare
  2. Either the IE BHO or Firefox extension (or both)

Installation:
  1. Build the IE BHO, or call regsvr32 on a prebuilt binary
  2. Add a file called "measurepageloadtimeextension@google.com" to
     the default Firefox profile directory under extensions, containing
     the path to the Firefox extension root

Invoke with the command line arguments as documented within
the command line.
"""

import command_line
import win32process

from drivers import windowing
from utils import browser_iterate

def CreateCommand(cmdline):
  """Inserts the command and arguments into a command line for parsing."""
  cmd = cmdline.AddCommand(
    ["measure"],
    "Measures how long a series of URLs takes to load in one or more browsers.",
    None,
    ExecuteMeasure)

  browser_iterate.SetupIterationCommandLine(cmd)
  cmd.AddArgument(
    ["-log", "--logfile"], "File to write output", type="string", required=True)


def ExecuteMeasure(command):
  """Executes the Measure command."""
  
  def LogResult(url, proc, wnd, result):
    """Write the result of the browse to the log file."""
    log_file.write(result)

  log_file = open(command["--logfile"], "w")

  browser_iterate.Iterate(command, LogResult)  

  # Close the log file and return. We're done.
  log_file.close()
