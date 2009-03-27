#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# purify_test.py

'''Runs an exe through Valgrind and puts the intermediate files in a
directory.
'''

import datetime
import glob
import logging
import optparse
import os
import re
import shutil
import stat
import sys
import tempfile

import common

import valgrind_analyze

import google.logging_utils

rmtree = shutil.rmtree

class Valgrind(object):

  """Abstract class for running Valgrind.

  Always subclass this and implement ValgrindCommand() with platform specific
  stuff.
  """

  TMP_DIR = "valgrind.tmp"

  def __init__(self):
    # If we have a valgrind.tmp directory, we failed to cleanup last time.
    if os.path.exists(self.TMP_DIR):
      shutil.rmtree(self.TMP_DIR)
    os.mkdir(self.TMP_DIR)

  def CreateOptionParser(self):
    self._parser = optparse.OptionParser("usage: %prog [options] <program to "
                                         "test>")
    self._parser.add_option("-t", "--timeout",
                      dest="timeout", metavar="TIMEOUT", default=10000,
                      help="timeout in seconds for the run (default 10000)")
    self._parser.add_option("", "--source_dir",
                            help="path to top of source tree for this build"
                                 "(used to normalize source paths in baseline)")
    self._parser.add_option("", "--suppressions", default=["."],
                            action="append",
                            help="path to a valgrind suppression file")
    self._parser.add_option("", "--gtest_filter", default="",
                            help="which test case to run")
    self._parser.add_option("", "--gtest_print_time", action="store_true",
                            default=False,
                            help="show how long each test takes")
    self._parser.add_option("", "--trace_children", action="store_true",
                            default=False,
                            help="also trace child processes")
    self._parser.add_option("", "--indirect", action="store_true",
                            default=False,
                            help="set BROWSER_WRAPPER rather than running valgrind directly")
    self._parser.add_option("", "--show_all_leaks", action="store_true",
                            default=False,
                            help="also show less blatant leaks")
    self._parser.add_option("", "--track_origins", action="store_true",
                            default=False,
                            help="Show whence uninit bytes came.  30% slower.")
    self._parser.add_option("", "--generate_suppressions", action="store_true",
                            default=False,
                            help="Skip analysis and generate suppressions")
    self._parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")
    self._parser.description = __doc__

  def ParseArgv(self):
    self.CreateOptionParser()
    self._options, self._args = self._parser.parse_args()
    self._timeout = int(self._options.timeout)
    self._suppressions = self._options.suppressions
    self._generate_suppressions = self._options.generate_suppressions
    self._source_dir = self._options.source_dir
    if self._options.gtest_filter != "":
      self._args.append("--gtest_filter=%s" % self._options.gtest_filter)
    if self._options.gtest_print_time:
      self._args.append("--gtest_print_time");
    if self._options.verbose:
      google.logging_utils.config_root(logging.DEBUG)
    else:
      google.logging_utils.config_root()

    return True

  def Setup(self):
    return self.ParseArgv()

  def PrepareForTest(self):
    """Perform necessary tasks prior to executing the test."""
    pass

  def ValgrindCommand(self):
    """Get the valgrind command to run."""
    raise RuntimeError, "Never use Valgrind directly. Always subclass and " \
                        "implement ValgrindCommand() at least"

  def Execute(self):
    ''' Execute the app to be tested after successful instrumentation.
    Full execution command-line provided by subclassers via proc.'''
    logging.info("starting execution...")

    proc = self.ValgrindCommand()
    os.putenv("G_SLICE", "always-malloc")
    logging.info("export G_SLICE=always-malloc");
    os.putenv("NSS_DISABLE_ARENA_FREE_LIST", "1")
    logging.info("export NSS_DISABLE_ARENA_FREE_LIST=1");

    common.RunSubprocess(proc, self._timeout)

    # Always return true, even if running the subprocess failed. We depend on
    # Analyze to determine if the run was valid. (This behaviour copied from
    # the purify_test.py script.)
    return True

  def Analyze(self):
    # Glob all the files in the "valgrind.tmp" directory
    filenames = glob.glob(self.TMP_DIR + "/valgrind.*")
    analyzer = valgrind_analyze.ValgrindAnalyze(self._source_dir, filenames, self._options.show_all_leaks)
    return analyzer.Report()

  def Cleanup(self):
    # Right now, we can cleanup by deleting our temporary directory. Other
    # cleanup is still a TODO?
    shutil.rmtree(self.TMP_DIR)
    return True

  def RunTestsAndAnalyze(self):
    self.PrepareForTest()

    self.Execute()
    if self._generate_suppressions:
      logging.info("Skipping analysis to let you look at the raw output...")
      return 0

    retcode = self.Analyze()
    if retcode:
      logging.error("Analyze failed.")
      return retcode
    logging.info("Execution and analysis completed successfully.")
    return 0

  def Main(self):
    '''Call this to run through the whole process: Setup, Execute, Analyze'''
    start = datetime.datetime.now()
    retcode = -1
    if self.Setup():
      retcode = self.RunTestsAndAnalyze()

      # Skip cleanup on generate.
      if not self._generate_suppressions:
        self.Cleanup()
    else:
      logging.error("Setup failed")
    end = datetime.datetime.now()
    seconds = (end - start).seconds
    hours = seconds / 3600
    seconds = seconds % 3600
    minutes = seconds / 60
    seconds = seconds % 60
    logging.info("elapsed time: %02d:%02d:%02d" % (hours, minutes, seconds))
    return retcode


class ValgrindLinux(Valgrind):

  """Valgrind on Linux."""

  def __init__(self):
    Valgrind.__init__(self)

  def ValgrindCommand(self):
    """Get the valgrind command to run."""
    # note that self._args begins with the exe to be run
    proc = ["valgrind", "--smc-check=all", "--leak-check=full",
            "--num-callers=30"]

    if self._options.show_all_leaks:
      proc += ["--show-reachable=yes"];

    if self._options.track_origins:
      proc += ["--track-origins=yes"];

    if self._options.trace_children:
      proc += ["--trace-children=yes"];

    # Either generate suppressions or load them.
    if self._generate_suppressions:
      proc += ["--gen-suppressions=all"]
    else:
      proc += ["--xml=yes"]

    suppression_count = 0
    for suppression_file in self._suppressions:
      if os.path.exists(suppression_file):
        suppression_count += 1
        proc += ["--suppressions=%s" % suppression_file]

    if not suppression_count:
      logging.warning("WARNING: NOT USING SUPPRESSIONS!")

    proc += ["--log-file=" + self.TMP_DIR + "/valgrind.%p"]

    if self._options.indirect:
      # The program being run invokes Python or something else
      # that can't stand to be valgrinded, and also invokes
      # the Chrome browser.  Set an environment variable to
      # tell the program to prefix the Chrome commandline
      # with a magic wrapper.  Build the magic wrapper here.
      (fd, indirect_fname) = tempfile.mkstemp(dir=self.TMP_DIR, prefix="browser_wrapper.", text=True)
      f = os.fdopen(fd, "w");
      f.write("#!/bin/sh\n")
      f.write(" ".join(proc))
      f.write(' "$@"\n')
      f.close()
      os.chmod(indirect_fname, stat.S_IRUSR|stat.S_IXUSR)
      os.putenv("BROWSER_WRAPPER", indirect_fname)
      logging.info('export BROWSER_WRAPPER=' + indirect_fname);
      proc = []

    proc += self._args
    return proc


class ValgrindMac(Valgrind):

  """Valgrind on Mac OS X.

  Valgrind on OS X does not support suppressions (yet).
  """

  def __init__(self):
    Valgrind.__init__(self)

  def PrepareForTest(self):
    """Runs dsymutil if needed.

    Valgrind for Mac OS X requires that debugging information be in a .dSYM
    bundle generated by dsymutil.  It is not currently able to chase DWARF
    data into .o files like gdb does, so executables without .dSYM bundles or
    with the Chromium-specific "fake_dsym" bundles generated by
    build/mac/strip_save_dsym won't give source file and line number
    information in valgrind.

    This function will run dsymutil if the .dSYM bundle is missing or if
    it looks like a fake_dsym.  A non-fake dsym that already exists is assumed
    to be up-to-date.
    """

    test_command = self._args[0]
    dsym_bundle = self._args[0] + '.dSYM'
    dsym_file = os.path.join(dsym_bundle, 'Contents', 'Resources', 'DWARF',
                             os.path.basename(test_command))
    dsym_info_plist = os.path.join(dsym_bundle, 'Contents', 'Info.plist')

    needs_dsymutil = True
    saved_test_command = None

    if os.path.exists(dsym_file) and os.path.exists(dsym_info_plist):
      # Look for the special fake_dsym tag in dsym_info_plist.
      dsym_info_plist_contents = open(dsym_info_plist).read()

      if not re.search('^\s*<key>fake_dsym</key>$', dsym_info_plist_contents,
                       re.MULTILINE):
        # fake_dsym is not set, this is a real .dSYM bundle produced by
        # dsymutil.  dsymutil does not need to be run again.
        needs_dsymutil = False
      else:
        # fake_dsym is set.  dsym_file is a copy of the original test_command
        # before it was stripped.  Copy it back to test_command so that
        # dsymutil has unstripped input to work with.  Move the stripped
        # test_command out of the way, it will be restored when this is
        # done.
        saved_test_command = test_command + '.stripped'
        os.rename(test_command, saved_test_command)
        shutil.copyfile(dsym_file, test_command)

    if needs_dsymutil:
      # Remove the .dSYM bundle if it exists.
      shutil.rmtree(dsym_bundle, True)

      dsymutil_command = ['dsymutil', test_command]

      # dsymutil is crazy slow.  Let it run for up to a half hour.  I hope
      # that's enough.
      common.RunSubprocess(dsymutil_command, 30 * 60)

      if saved_test_command:
        os.rename(saved_test_command, test_command)

  def ValgrindCommand(self):
    """Get the valgrind command to run."""
    proc = ["valgrind", "--smc-check=all", "--leak-check=full",
            "--num-callers=30"]

    if self._options.show_all_leaks:
      proc += ["--show-reachable=yes"];

    # Either generate suppressions or load them.
    # TODO(nirnimesh): Enable when Analyze() is implemented
    #if self._generate_suppressions:
    #  proc += ["--gen-suppressions=all"]
    #else:
    #  proc += ["--xml=yes"]

    suppression_count = 0
    for suppression_file in self._suppressions:
      if os.path.exists(suppression_file):
        suppression_count += 1
        proc += ["--suppressions=%s" % suppression_file]

    if not suppression_count:
      logging.warning("WARNING: NOT USING SUPPRESSIONS!")

    # TODO(nirnimesh): Enable --log-file when Analyze() is implemented
    # proc += ["--log-file=" + self.TMP_DIR + "/valgrind.%p"]

    proc += self._args
    return proc

  def Analyze(self):
    # TODO(nirnimesh): Implement analysis later. Valgrind on Mac is new so
    # analysis might not be useful until we have stable output from valgrind
    return 0


if __name__ == "__main__":
  if sys.platform == 'darwin': # Mac
    valgrind = ValgrindMac()
    retcode = valgrind.Main()
    sys.exit(retcode)
  elif sys.platform == 'linux2': # Linux
    valgrind = ValgrindLinux()
    retcode = valgrind.Main()
    sys.exit(retcode)
  else:
    logging.error("Unknown platform: %s" % sys.platform)
    sys.exit(1)
