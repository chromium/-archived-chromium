#!/usr/bin/python 
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

"""Script to verify a Portable Executable's dependencies.

Analyzes the input portable executable (a DLL or an EXE for example), extracts
its imports and confirms that its dependencies haven't changed. This is for
regression testing.

Returns 0 if the list matches.
        1 if one or more dependencies has been removed.
        2 if one or more dependencies has been added. This preempts removal
          result code.
"""

import optparse
import os
import subprocess
import sys

DUMPBIN = "dumpbin.exe"


class Error(Exception):
  def __init__(self, message):
    self.message = message


def RunSystemCommand(cmd):
  return subprocess.Popen(cmd, stdout=subprocess.PIPE).communicate()[0]


def RunDumpbin(binary_file):
  """Runs dumpbin and parses its output.

  Args: binary_file: the binary to analyze
  Returns: a tuple of the dependencies and the delay-load dependencies

  The output of dumpbin that we will be parsing looks like this:
  --
  <blah blah>

  Image has the following dependencies:

  foo.dll
  bar.dll

  Image has the following delay load dependencies:

  foobar.dll
  other.dll

  Summary

  <blah blah>
  --
  The following parser extracts the dll names from the above format.
  """
  cmd = DUMPBIN + " /dependents " + binary_file
  output = RunSystemCommand(cmd)
  dependents = []
  delay_loaded = []
  (START, DEPENDENCIES_HEADER, DEPENDENCIES, DELAY_LOAD_HEADER, DELAY_LOAD,
   SUMMARY_HEADER, SUMMARY) = (0, 1, 2, 3, 4, 5, 6)
  current_section = START
  # Very basic scanning.
  for line in output.splitlines():
    line = line.strip()
    if len(line) > 1:
      if line == "Image has the following dependencies:":
        if current_section != START:
          raise Error("Internal parsing error.")
        current_section = DEPENDENCIES_HEADER;
      elif line == "Image has the following delay load dependencies:":
        if current_section != DEPENDENCIES:
          raise Error("Internal parsing error.")
        current_section = DELAY_LOAD_HEADER;
      elif line == "Summary":
        current_section = SUMMARY_HEADER;
      elif current_section == DEPENDENCIES:
        # Got a dependent
        dependents.append(line)
      elif current_section == DELAY_LOAD:
        # Got a delay-loaded
        delay_loaded.append(line)
    else:
      if current_section == DEPENDENCIES_HEADER:
        current_section = DEPENDENCIES
      elif current_section == DELAY_LOAD_HEADER:
        current_section = DELAY_LOAD
      elif current_section == SUMMARY_HEADER:
        current_section = SUMMARY
  return dependents, delay_loaded


def Diff(name, type, current, expected, deps_file):
  """
  Args: name: Portable executable name being analysed.
        type: Type of dependency.
        current: List of current dependencies.
        expected: List of dependencies that are expected.
        deps_file: File name of the .deps file.
  Returns 0 if the lists are equal
          1 if one entry in list1 is missing
          2 if one entry in list2 is missing.
  """
  # Create sets of lower-case names.
  set_expected = set([x.lower() for x in expected])
  set_current = set([x.lower() for x in current])
  only_in_expected = set_expected - set_current
  only_in_current = set_current - set_expected

  # Find difference between the sets.
  found_extra = 0
  name = os.path.basename(name).lower()
  if len(only_in_expected) or len(only_in_current):
    print name.upper() + " DEPENDENCIES MISMATCH\n"

  if len(only_in_current):
    found_extra = 1
    print "%s is no longer dependent on these %s: %s." % (name,
        type,
        ' '.join(only_in_current))
    print "Please update \"%s\"." % deps_file

  if len(only_in_expected):
    found_extra = 2
    string = "%s is now dependent on these %s, but shouldn't: %s." % (name,
        type,
        ' '.join(only_in_expected))
    stars = '*' * len(string)
    print "**" + stars + "**"
    print "* " + string + " *"
    print "**" + stars + "**\n"
    print "Please update \"%s\"." % deps_file
  return found_extra


def VerifyDependents(pe_name, dependents, delay_loaded, list_file):
  """Compare the actual dependents to the expected ones."""
  scope = {}
  execfile(list_file, scope)
  deps_result = Diff(pe_name,
                     "dll",
                     dependents,
                     scope["dependents"],
                     list_file)
  delayed_result = Diff(pe_name,
                        "delay loaded dll",
                        delay_loaded,
                        scope["delay_loaded"],
                        list_file)
  return max(deps_result, delayed_result)


def main(options, args):
  # PE means portable executable. It's any .DLL, .EXE, .SYS, .AX, etc.
  pe_name = args[0]
  deps_file = args[1]
  dependents, delay_loaded = RunDumpbin(pe_name)
  return VerifyDependents(pe_name, dependents, delay_loaded, deps_file)


if '__main__' == __name__:
  usage = "usage: %prog [options] input output"
  option_parser = optparse.OptionParser(usage = usage)
  options, args = option_parser.parse_args()
  if len(args) != 2:
      parser.error("Incorrect number of arguments")
  sys.exit(main(options, args))
