#!/bin/env python
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

# chrome_tests.py

''' Runs various chrome tests through purify_test.py
'''

import logging
import optparse
import os
import stat
import sys

import google.logging_utils
import google.path_utils
import google.platform_utils

import common

class TestNotFound(Exception): pass

class ChromeTests:

  def __init__(self, options, args, test):
    # the known list of tests
    self._test_list = {"test_shell": self.TestTestShell,
                       "unit": self.TestUnit,
                       "net": self.TestNet,
                       "ipc": self.TestIpc,
                       "base": self.TestBase,
                       "layout": self.TestLayout,
                       "ui": self.TestUI}

    if test not in self._test_list:
      raise TestNotFound("Unknown test: %s" % test)

    self._options = options
    self._args = args
    self._test = test

    script_dir = google.path_utils.ScriptDir()
    utility = google.platform_utils.PlatformUtility(script_dir)
    # Compute the top of the tree (the "source dir") from the script dir (where
    # this script lives).  We assume that the script dir is in tools/purify
    # relative to the top of the tree.
    self._source_dir = os.path.dirname(os.path.dirname(script_dir))
    # since this path is used for string matching, make sure it's always
    # an absolute Windows-style path
    self._source_dir = utility.GetAbsolutePath(self._source_dir)
    purify_test = os.path.join(script_dir, "purify_test.py")
    self._command_preamble = ["python.exe", purify_test, "--echo_to_stdout", 
                              "--source_dir=%s" % (self._source_dir),
                              "--save_cache"]

  def _DefaultCommand(self, module, exe=None):
    '''Generates the default command array that most tests will use.'''
    module_dir = os.path.join(self._source_dir, module)
    if module == "chrome":
      # unfortunately, not all modules have the same directory structure
      self._data_dir = os.path.join(module_dir, "test", "data", "purify")
    else:
      self._data_dir = os.path.join(module_dir, "data", "purify")
    if not self._options.build_dir:
      dir_chrome = os.path.join(self._source_dir, "chrome", "Release")
      dir_module = os.path.join(module_dir, "Release")
      if exe:
        exe_chrome = os.path.join(dir_chrome, exe)
        exe_module = os.path.join(dir_module, exe)
        if os.path.isfile(exe_chrome) and not os.path.isfile(exe_module):
          self._options.build_dir = dir_chrome
        elif os.path.isfile(exe_module) and not os.path.isfile(exe_chrome):
          self._options.build_dir = dir_module
        elif os.stat(exe_module)[stat.ST_MTIME] > os.stat(exe_chrome)[stat.ST_MTIME]:
          self._options.build_dir = dir_module
        else:
          self._options.build_dir = dir_chrome
      else:
        if os.path.isdir(dir_chrome) and not os.path.isdir(dir_module):
          self._options.build_dir = dir_chrome
        elif os.path.isdir(dir_module) and not os.path.isdir(dir_chrome):
          self._options.build_dir = dir_module
        elif os.stat(dir_module)[stat.ST_MTIME] > os.stat(dir_chrome)[stat.ST_MTIME]:
          self._options.build_dir = dir_module
        else:
          self._options.build_dir = dir_chrome

    cmd = self._command_preamble
    cmd.append("--data_dir=%s" % self._data_dir)
    if self._options.baseline:
      cmd.append("--baseline")
    if self._options.verbose:
      cmd.append("--verbose")
    if exe:
      cmd.append(os.path.join(self._options.build_dir, exe))
    return cmd

  def Run(self):
    ''' Runs the test specified by command-line argument --test '''
    logging.info("running test %s" % (self._test))
    return self._test_list[self._test]()
    
  def _ReadGtestFilterFile(self, name, cmd):
    '''Read a file which is a list of tests to filter out with --gtest_filter
    and append the command-line option to cmd.
    '''
    filters = []
    filename = os.path.join(self._data_dir, name + ".gtest.txt")
    if os.path.exists(filename):
      f = open(filename, 'r')
      for line in f.readlines():
        if line.startswith("#") or line.startswith("//") or line.isspace():
          continue
        line = line.rstrip()
        filters.append(line)
    gtest_filter = self._options.gtest_filter
    if len(filters):
      if gtest_filter:
        gtest_filter += ":"
        if gtest_filter.find("-") < 0:
          gtest_filter += "-"
      else:
        gtest_filter = "-"
      gtest_filter += ":".join(filters)
    if gtest_filter:
      cmd.append("--gtest_filter=%s" % gtest_filter)

  def SimpleTest(self, module, name):
    cmd = self._DefaultCommand(module, name)
    self._ReadGtestFilterFile(name, cmd)
    return common.RunSubprocess(cmd, 0)

  def ScriptedTest(self, module, exe, name, script, multi=False, cmd_args=None):
    '''Purify a target exe, which will be executed one or more times via a 
       script or driver program.
    Args:
      module - which top level component this test is from (webkit, base, etc.)
      exe - the name of the exe (it's assumed to exist in build_dir)
      name - the name of this test (used to name output files)
      script - the driver program or script.  If it's python.exe, we use 
        search-path behavior to execute, otherwise we assume that it is in
        build_dir.
      multi - a boolean hint that the exe will be run multiple times, generating
        multiple output files (without this option, only the last run will be
        recorded and analyzed)
      cmd_args - extra arguments to pass to the purify_test.py script
    '''
    cmd = self._DefaultCommand(module)
    exe = os.path.join(self._options.build_dir, exe)
    cmd.append("--exe=%s" % exe)
    cmd.append("--name=%s" % name)
    if multi:
      out = os.path.join(google.path_utils.ScriptDir(),
                         "latest", "%s%%5d.txt" % name)
      cmd.append("--out_file=%s" % out)
    if cmd_args:
      cmd.extend(cmd_args)
    if script[0] != "python.exe" and not os.path.exists(script[0]):
      script[0] = os.path.join(self._options.build_dir, script[0])
    cmd.extend(script)
    self._ReadGtestFilterFile(name, cmd)
    return common.RunSubprocess(cmd, 0)

  def TestBase(self):
    return self.SimpleTest("base", "base_unittests.exe")

  def TestIpc(self):
    return self.SimpleTest("chrome", "ipc_tests.exe")
    
  def TestNet(self):
    return self.SimpleTest("net", "net_unittests.exe")

  def TestTestShell(self):
    return self.SimpleTest("webkit", "test_shell_tests.exe")

  def TestUnit(self):
    return self.SimpleTest("chrome", "unit_tests.exe")

  def TestLayout(self):
    script = os.path.join(self._source_dir, "webkit", "tools", "layout_tests",
                          "run_webkit_tests.py")
    script_cmd = ["python.exe", script, "--run-singly", "-v",
                  "--noshow-results", "--time-out-ms=200000"]
    if len(self._args):
      # if the arg is a txt file, then treat it as a list of tests
      if os.path.isfile(self._args[0]) and self._args[0][-4:] == ".txt":
        script_cmd.append("--test-list=%s" % self._args[0])
      else:
        script_cmd.extend(self._args)
    self.ScriptedTest("webkit", "test_shell.exe", "layout",
        script_cmd, multi=True, cmd_args=["--timeout=0"])
    # since layout tests take so long to run, having the test red on buildbot
    # isn't very useful
    return 0

  def TestUI(self):
    return self.ScriptedTest("chrome", "chrome.exe", "ui_tests", 
        ["ui_tests.exe", "--single-process", "--test-timeout=100000000"], multi=True)

def _main(argv):
  parser = optparse.OptionParser("usage: %prog -b <dir> -t <test> "
                                 "[-t <test> ...]")
  parser.disable_interspersed_args()
  parser.add_option("-b", "--build_dir",
                    help="the location of the output of the compiler output")
  parser.add_option("-t", "--test", action="append",
                    help="which test to run")
  parser.add_option("", "--baseline", action="store_true", default=False,
                    help="generate baseline data instead of validating")
  parser.add_option("", "--gtest_filter",
                    help="additional arguments to --gtest_filter")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")
  (options, args) = parser.parse_args()

  if options.verbose:
    google.logging_utils.config_root(logging.DEBUG)
  else:
    google.logging_utils.config_root()

  if not options.test or not len(options.test):
    parser.error("--test not specified")

  for t in options.test:
    tests = ChromeTests(options, args, t)
    ret = tests.Run()
    if ret: return ret
  return 0

if __name__ == "__main__":
  ret = _main(sys.argv)
  sys.exit(ret)
