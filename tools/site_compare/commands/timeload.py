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

"""SiteCompare command to time page loads

Loads a series of URLs in a series of browsers (and browser versions)
and measures how long the page takes to load in each. Outputs a 
comma-delimited file. The first line is "URL,[browser names", each
additional line is a URL follored by comma-delimited times (in seconds),
or the string "timeout" or "crashed".

"""

import os            # Functions for walking the directory tree
import tempfile      # Get a temporary directory to hold intermediates

import command_line
import drivers       # Functions for driving keyboard/mouse/windows, OS-specific
import operators     # Functions that, given two bitmaps as input, produce
                     # output depending on the performance of an operation
import scrapers      # Functions that know how to capture a render from
                     # particular browsers


def CreateCommand(cmdline):
  """Inserts the command and arguments into a command line for parsing."""
  cmd = cmdline.AddCommand(
    ["timeload"],
    "Measures how long a series of URLs takes to load in one or more browsers.",
    None,
    ExecuteTimeLoad)

  cmd.AddArgument(
    ["-b", "--browsers"], "List of browsers to use. Comma-separated",
    type="string", required=True)
  cmd.AddArgument(
    ["-bp", "--browserpaths"], "List of paths to browsers. Comma-separated",
    type="string", required=False)
  cmd.AddArgument(
    ["-bv", "--browserversions"], "List of versions of browsers. Comma-separated",
    type="string", required=False)
  cmd.AddArgument(
    ["-u", "--url"], "URL to time")
  cmd.AddArgument(
    ["-l", "--list"], "List of URLs to time", type="readfile")
  cmd.AddMutualExclusion(["--url", "--list"])
  cmd.AddArgument(
    ["-s", "--startline"], "First line of URL list", type="int")
  cmd.AddArgument(
    ["-e", "--endline"], "Last line of URL list (exclusive)", type="int")
  cmd.AddArgument(
    ["-c", "--count"], "Number of lines of URL file to use", type="int")
  cmd.AddDependency("--startline", "--list")
  cmd.AddRequiredGroup(["--url", "--list"])
  cmd.AddDependency("--endline", "--list")
  cmd.AddDependency("--count", "--list")
  cmd.AddMutualExclusion(["--count", "--endline"])
  cmd.AddDependency("--count", "--startline")
  cmd.AddArgument(
    ["-t", "--timeout"], "Amount of time (seconds) to wait for browser to "
    "finish loading",
    type="int", default=60)
  cmd.AddArgument(
    ["-log", "--logfile"], "File to write output", type="string", required=True)
  cmd.AddArgument(
    ["-sz", "--size"], "Browser window size", default=(800, 600), type="coords")

  
def ExecuteTimeLoad(command):
  """Executes the TimeLoad command."""
  browsers = command["--browsers"].split(",")
  num_browsers = len(browsers)
  
  if command["--browserversions"]:
    browser_versions = command["--browserversions"].split(",")
  else:
    browser_versions = [None] * num_browsers
    
  if command["--browserpaths"]:
    browser_paths = command["--browserpaths"].split(",")
  else:
    browser_paths = [None] * num_browsers
  
  if len(browser_versions) != num_browsers:
    raise ValueError(
      "--browserversions must be same length as --browser_paths")
  if len(browser_paths) != num_browsers:
    raise ValueError(
      "--browserversions must be same length as --browser_paths")
      
  if [b for b in browsers if b not in ["chrome", "ie", "firefox"]]:
    raise ValueError("unknown browsers: %r" % b)
    
  scraper_list = []
  
  for b in xrange(num_browsers):
    version = browser_versions[b]
    if not version: version = None
    
    scraper = scrapers.GetScraper( (browsers[b], version) )
    if not scraper:
      raise ValueError("could not find scraper for (%r, %r)" % 
        (browsers[b], version))
    scraper_list.append(scraper)
    
  if command["--url"]:
    url_list = [command["--url"]]
  else:
    startline = command["--startline"]
    if command["--count"]:
      endline = startline+command["--count"]
    else:
      endline = command["--endline"]
    url_list = [url.strip() for url in
                open(command["--list"], "r").readlines()[startline:endline]]
    
  log_file = open(command["--logfile"], "w")
  
  log_file.write("URL")
  for b in xrange(num_browsers):
    log_file.write(",%s" % browsers[b])
    
    if browser_versions[b]: log_file.write(" %s" % browser_versions[b])
  log_file.write("\n")
  
  results = {}
  for url in url_list:
    results[url] = [None] * num_browsers
  
  for b in xrange(num_browsers):
    result = scraper_list[b].Time(url_list, command["--size"],
      command["--timeout"],
      path=browser_paths[b])
    
    for (url, time) in result:
      results[url][b] = time
      
  # output the results
  for url in url_list:
    log_file.write(url)
    for b in xrange(num_browsers):
      log_file.write(",%r" % results[url][b])
  
