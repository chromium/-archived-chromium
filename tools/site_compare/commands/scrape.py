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

"""Command for scraping images from a URL or list of URLs.

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

from drivers import windowing
from utils import browser_iterate

def CreateCommand(cmdline):
  """Inserts the command and arguments into a command line for parsing."""
  cmd = cmdline.AddCommand(
    ["scrape"],
    "Scrapes an image from a URL or series of URLs.",
    None,
    ExecuteScrape)

  browser_iterate.SetupIterationCommandLine(cmd)
  cmd.AddArgument(
    ["-log", "--logfile"], "File to write text output", type="string")
  cmd.AddArgument(
    ["-out", "--outdir"], "Directory to store scrapes", type="string", required=True)


def ExecuteScrape(command):
  """Executes the Scrape command."""
  
  def ScrapeResult(url, proc, wnd, result):
    """Capture and save the scrape."""
    if log_file: log_file.write(result)

    # Scrape the page
    image = windowing.ScrapeWindow(wnd)
    filename = windowing.URLtoFilename(url, command["--outdir"], ".bmp")
    image.save(filename)    

  if command["--logfile"]: log_file = open(command["--logfile"], "w")
  else: log_file = None

  browser_iterate.Iterate(command, ScrapeResult)  

  # Close the log file and return. We're done.
  if log_file: log_file.close()
