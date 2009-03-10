#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

# The default distribution name and the environment variable that overrides it.
DIST_DEFAULT = '_chromium'
DIST_ENV_VAR = 'CHROMIUM_BUILD'

DUMPBIN = "dumpbin.exe"


class Error(Exception):
  def __init__(self, message):
    self.message = message
  def __str__(self):
    return self.message


def RunSystemCommand(cmd):
  try:
    return subprocess.Popen(cmd, stdout=subprocess.PIPE).communicate()[0]
  except:
    raise Error("Failed to execute: " + cmd)

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
        current_section = DEPENDENCIES_HEADER
      elif line == "Image has the following delay load dependencies:":
        if current_section != DEPENDENCIES:
          raise Error("Internal parsing error.")
        current_section = DELAY_LOAD_HEADER
      elif line == "Summary":
        current_section = SUMMARY_HEADER
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

  if len(only_in_expected):
    # Setting found_extra to 1 causes the build to fail. In some case, some
    # dependencies are stripped out on optimized build; don't break anything
    # just for that.
    found_extra = 0
    print "%s is no longer dependent on these %s: %s." % (name,
        type,
        ' '.join(only_in_expected))
    print "Please update \"%s\"." % deps_file

  if len(only_in_current):
    found_extra = 2
    string = "%s is now dependent on these %s, but shouldn't: %s." % (name,
        type,
        ' '.join(only_in_current))
    stars = '*' * len(string)
    print "**" + stars + "**"
    print "* " + string + " *"
    print "**" + stars + "**\n"
    print "Please update \"%s\"." % deps_file
  return found_extra


def VerifyDependents(pe_name, dependents, delay_loaded, list_file, verbose):
  """Compare the actual dependents to the expected ones."""
  scope = {}
  try:
    execfile(list_file, scope)
  except:
    raise Error("Failed to load " + list_file)

  # The dependency files have dependencies in two section - dependents and
  # delay_loaded. Also various distributions of Chromium can have different
  # dependencies. So first we read generic dependencies ("dependents" and
  # "delay_loaded"). If distribution specific dependencies exist
  # (i.e. "dependents_google_chrome" and "delay_loaded_google_chrome") we use
  # those instead.
  distribution = DIST_DEFAULT
  if DIST_ENV_VAR in os.environ.keys():
    distribution = os.environ[DIST_ENV_VAR].lower()

  expected_dependents = scope["dependents"]
  dist_dependents = "dependents" + distribution
  if dist_dependents in scope.keys():
    expected_dependents = scope[dist_dependents]

  expected_delay_loaded = scope["delay_loaded"]
  dist_delay_loaded = "delay_loaded" + distribution
  if dist_delay_loaded in scope.keys():
    expected_delay_loaded = scope[dist_delay_loaded]

  if verbose:
    print "Expected dependents:"
    print "\n".join(expected_dependents)
    print "Expected delayloaded:"
    print "\n".join(expected_delay_loaded)

  deps_result = Diff(pe_name,
                     "dll",
                     dependents,
                     expected_dependents,
                     list_file)
  delayed_result = Diff(pe_name,
                        "delay loaded dll",
                        delay_loaded,
                        expected_delay_loaded,
                        list_file)
  return max(deps_result, delayed_result)


def main(options, args):
  # PE means portable executable. It's any .DLL, .EXE, .SYS, .AX, etc.
  pe_name = args[0]
  deps_file = args[1]
  dependents, delay_loaded = RunDumpbin(pe_name)
  if options.debug:
    print "Dependents:"
    print "\n".join(dependents)
    print "Delayloaded:"
    print "\n".join(delay_loaded)
  return VerifyDependents(pe_name, dependents, delay_loaded, deps_file,
                          options.debug)


if '__main__' == __name__:
  usage = "usage: %prog [options] input output"
  option_parser = optparse.OptionParser(usage = usage)
  option_parser.add_option("-d",
                           "--debug",
                           dest="debug",
                           action="store_true",
                           default=False,
                           help="Display debugging information")
  options, args = option_parser.parse_args()
  if len(args) != 2:
    option_parser.error("Incorrect number of arguments")
  sys.exit(main(options, args))
