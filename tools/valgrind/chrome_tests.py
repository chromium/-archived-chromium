#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# chrome_tests.py

''' Runs various chrome tests through valgrind_test.py.

This file is a copy of ../purify/chrome_tests.py. Eventually, it would be nice
to merge these two files. For now, I'm leaving it here with sections that
aren't supported commented out as this is more of a work in progress.
'''

import glob
import logging
import optparse
import os
import stat
import sys

import google.logging_utils
import google.path_utils
# Import the platform_utils up in the layout tests which have been modified to
# work under non-Windows platforms instead of the ones that are in the
# tools/python/google directory. (See chrome_tests.sh which sets PYTHONPATH
# correctly.)
#
# TODO(erg): Copy/Move the relevant functions from the layout_package version
# of platform_utils back up to google.platform_utils
# package. http://crbug.com/6164
import layout_package.platform_utils

import common


class TestNotFound(Exception): pass


class ChromeTests:
  '''This class is derived from the chrome_tests.py file in ../purify/.

  TODO(erg): Finish implementing this. I've commented out all the parts that I
  don't have working yet. We still need to deal with layout tests, and long
  term, the UI tests.
  '''

  def __init__(self, options, args, test):
    # the known list of tests
    self._test_list = {
      "test_shell": self.TestTestShell,
      "unit": self.TestUnit,
      "net": self.TestNet,
      "ipc": self.TestIpc,
      "base": self.TestBase,
      "googleurl": self.TestGoogleurl,
      "media": self.TestMedia,
      "printing": self.TestPrinting,
#      "layout": self.TestLayout,
#      "layout_all": self.TestLayoutAll,
      "ui": self.TestUI
    }

    if test not in self._test_list:
      raise TestNotFound("Unknown test: %s" % test)

    self._options = options
    self._args = args
    self._test = test

    script_dir = google.path_utils.ScriptDir()
    utility = layout_package.platform_utils.PlatformUtility(script_dir)
    # Compute the top of the tree (the "source dir") from the script dir (where
    # this script lives).  We assume that the script dir is in tools/valgrind/
    # relative to the top of the tree.
    self._source_dir = os.path.dirname(os.path.dirname(script_dir))
    # since this path is used for string matching, make sure it's always
    # an absolute Windows-style path
    self._source_dir = utility.GetAbsolutePath(self._source_dir)
    valgrind_test = os.path.join(script_dir, "valgrind_test.py")
    self._command_preamble = ["python", valgrind_test,
                              "--source_dir=%s" % (self._source_dir)]

  def _DefaultCommand(self, module, exe=None):
    '''Generates the default command array that most tests will use.'''
    module_dir = os.path.join(self._source_dir, module)

    # We need multiple data dirs, the current script directory and a module
    # specific one. The global suppression file lives in our directory, and the
    # module specific suppression file lives with the module.
    self._data_dirs = [google.path_utils.ScriptDir()]

    if module == "chrome":
      # unfortunately, not all modules have the same directory structure
      self._data_dirs.append(os.path.join(module_dir, "test", "data",
                                          "valgrind"))
    else:
      self._data_dirs.append(os.path.join(module_dir, "data", "valgrind"))

    if not self._options.build_dir:
      dir_chrome = os.path.join(self._source_dir, "chrome", "Hammer")
      dir_module = os.path.join(module_dir, "Hammer")
      if exe:
        exe_chrome = os.path.join(dir_chrome, exe)
        exe_module = os.path.join(dir_module, exe)
        if os.path.isfile(exe_chrome) and not os.path.isfile(exe_module):
          self._options.build_dir = dir_chrome
        elif os.path.isfile(exe_module) and not os.path.isfile(exe_chrome):
          self._options.build_dir = dir_module
        elif (os.stat(exe_module)[stat.ST_MTIME] >
              os.stat(exe_chrome)[stat.ST_MTIME]):
          self._options.build_dir = dir_module
        else:
          self._options.build_dir = dir_chrome
      else:
        if os.path.isdir(dir_chrome) and not os.path.isdir(dir_module):
          self._options.build_dir = dir_chrome
        elif os.path.isdir(dir_module) and not os.path.isdir(dir_chrome):
          self._options.build_dir = dir_module
        elif (os.stat(dir_module)[stat.ST_MTIME] >
              os.stat(dir_chrome)[stat.ST_MTIME]):
          self._options.build_dir = dir_module
        else:
          self._options.build_dir = dir_chrome

    cmd = list(self._command_preamble)
    for directory in self._data_dirs:
      suppression_file = os.path.join(directory, "suppressions.txt")
      if os.path.exists(suppression_file):
        cmd.append("--suppressions=%s" % suppression_file)
    if self._options.baseline:
      cmd.append("--baseline")
    if self._options.verbose:
      cmd.append("--verbose")
    if self._options.show_all_leaks:
      cmd.append("--show_all_leaks")
    if self._options.generate_suppressions:
      cmd.append("--generate_suppressions")
    if exe == "ui_tests":
      cmd.append("--trace_children")
      cmd.append("--indirect")
    if exe:
      cmd.append(os.path.join(self._options.build_dir, exe))
    # Valgrind runs tests slowly, so slow tests hurt more; show elapased time
    # so we can find the slowpokes.
    cmd.append("--gtest_print_time");
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
    for directory in self._data_dirs:
      filename = os.path.join(directory, name + ".gtest.txt")
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

  def SimpleTest(self, module, name, cmd_args=None):
    cmd = self._DefaultCommand(module, name)
    self._ReadGtestFilterFile(name, cmd)
    if cmd_args:
      cmd.extend(cmd_args)
    return common.RunSubprocess(cmd, 0)

  def ScriptedTest(self, module, exe, name, script, multi=False, cmd_args=None,
                   out_dir_extra=None):
    '''Valgrind a target binary, which will be executed one or more times via a
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
      cmd_args - extra arguments to pass to the valgrind_test.py script
    '''
    cmd = self._DefaultCommand(module)
    exe = os.path.join(self._options.build_dir, exe)
    cmd.append("--exe=%s" % exe)
    cmd.append("--name=%s" % name)
    if multi:
      out = os.path.join(google.path_utils.ScriptDir(),
                         "latest")
      if out_dir_extra:
        out = os.path.join(out, out_dir_extra)
        if os.path.exists(out):
          old_files = glob.glob(os.path.join(out, "*.txt"))
          for f in old_files:
            os.remove(f)
        else:
          os.makedirs(out)
      out = os.path.join(out, "%s%%5d.txt" % name)
      cmd.append("--out_file=%s" % out)
    if cmd_args:
      cmd.extend(cmd_args)
    if script[0] != "python.exe" and not os.path.exists(script[0]):
      script[0] = os.path.join(self._options.build_dir, script[0])
    cmd.extend(script)
    self._ReadGtestFilterFile(name, cmd)
    return common.RunSubprocess(cmd, 0)

  def TestBase(self):
    return self.SimpleTest("base", "base_unittests")

  def TestGoogleurl(self):
    return self.SimpleTest("chrome", "googleurl_unittests")

  def TestMedia(self):
    return self.SimpleTest("chrome", "media_unittests")

  def TestPrinting(self):
    return self.SimpleTest("chrome", "printing_unittests")

  def TestIpc(self):
    return self.SimpleTest("chrome", "ipc_tests")

  def TestNet(self):
    return self.SimpleTest("net", "net_unittests")

  def TestTestShell(self):
    return self.SimpleTest("webkit", "test_shell_tests")

  def TestUnit(self):
    return self.SimpleTest("chrome", "unit_tests")

  def TestUI(self):
    return self.SimpleTest("chrome", "ui_tests",
                           cmd_args=["--",
                            "--ui-test-timeout=120000",
                            "--ui-test-action-timeout=80000",
                            "--ui-test-action-max-timeout=180000"])

#   def TestLayoutAll(self):
#     return self.TestLayout(run_all=True)

#   def TestLayout(self, run_all=False):
#     # A "chunk file" is maintained in the local directory so that each test
#     # runs a slice of the layout tests of size chunk_size that increments with
#     # each run.  Since tests can be added and removed from the layout tests at
#     # any time, this is not going to give exact coverage, but it will allow us
#     # to continuously run small slices of the layout tests under purify rather
#     # than having to run all of them in one shot.
#     chunk_num = 0
#     # Tests currently seem to take about 20-30s each.
#     chunk_size = 120  # so about 40-60 minutes per run
#     chunk_file = os.path.join(os.environ["TEMP"], "purify_layout_chunk.txt")
#     if not run_all:
#       try:
#         f = open(chunk_file)
#         if f:
#           str = f.read()
#           if len(str):
#             chunk_num = int(str)
#           # This should be enough so that we have a couple of complete runs
#           # of test data stored in the archive (although note that when we loop
#           # that we almost guaranteed won't be at the end of the test list)
#           if chunk_num > 10000:
#             chunk_num = 0
#           f.close()
#       except IOError, (errno, strerror):
#         logging.error("error reading from file %s (%d, %s)" % (chunk_file,
#                       errno, strerror))

#     script = os.path.join(self._source_dir, "webkit", "tools", "layout_tests",
#                           "run_webkit_tests.py")
#     script_cmd = ["python.exe", script, "--run-singly", "-v",
#                   "--noshow-results", "--time-out-ms=200000",
#                   "--nocheck-sys-deps"]
#     if not run_all:
#       script_cmd.append("--run-chunk=%d:%d" % (chunk_num, chunk_size))

#     if len(self._args):
#       # if the arg is a txt file, then treat it as a list of tests
#       if os.path.isfile(self._args[0]) and self._args[0][-4:] == ".txt":
#         script_cmd.append("--test-list=%s" % self._args[0])
#       else:
#         script_cmd.extend(self._args)

#     if run_all:
#       ret = self.ScriptedTest("webkit", "test_shell.exe", "layout",
#                               script_cmd, multi=True, cmd_args=["--timeout=0"])
#       return ret

#     # store each chunk in its own directory so that we can find the data later
#     chunk_dir = os.path.join("layout", "chunk_%05d" % chunk_num)
#     ret = self.ScriptedTest("webkit", "test_shell.exe", "layout",
#                             script_cmd, multi=True, cmd_args=["--timeout=0"],
#                             out_dir_extra=chunk_dir)

#     # Wait until after the test runs to completion to write out the new chunk
#     # number.  This way, if the bot is killed, we'll start running again from
#     # the current chunk rather than skipping it.
#     try:
#       f = open(chunk_file, "w")
#       chunk_num += 1
#       f.write("%d" % chunk_num)
#       f.close()
#     except IOError, (errno, strerror):
#       logging.error("error writing to file %s (%d, %s)" % (chunk_file, errno,
#                     strerror))
#     # Since we're running small chunks of the layout tests, it's important to
#     # mark the ones that have errors in them.  These won't be visible in the
#     # summary list for long, but will be useful for someone reviewing this bot.
#     return ret

#   def TestUI(self):
#     if not self._options.no_reinstrument:
#       instrumentation_error = self.InstrumentDll()
#       if instrumentation_error:
#         return instrumentation_error
#     return self.ScriptedTest("chrome", "chrome.exe", "ui_tests",
#                              ["ui_tests.exe",
#                               "--single-process",
#                               "--ui-test-timeout=120000",
#                               "--ui-test-action-timeout=80000",
#                               "--ui-test-action-max-timeout=180000"],
#                              multi=True)


def _main(_):
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
  parser.add_option("", "--show_all_leaks", action="store_true",
                    default=False,
                    help="also show even less blatant leaks")
  parser.add_option("", "--no-reinstrument", action="store_true", default=False,
                    help="Don't force a re-instrumentation for ui_tests")
  parser.add_option("", "--generate_suppressions", action="store_true",
                    default=False,
                    help="Skip analysis and generate suppressions")

  options, args = parser.parse_args()

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
