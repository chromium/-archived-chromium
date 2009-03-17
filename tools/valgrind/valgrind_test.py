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
import shutil
import sys

import common

import valgrind_analyze

rmtree = shutil.rmtree

class Valgrind(object):

  """Abstract class for running Valgrind.

  Always subclass this and implement ValgrindCommand() with platform specific
  stuff.
  """

  TMP_DIR = "valgrind.tmp"

  def __init__(self):
    self._suppressions_files = []
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
    self._parser.add_option("", "--generate_suppressions", action="store_true",
                            default=False,
                            help="Skip analysis and generate suppressions")
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
    return True

  def Setup(self):
    return self.ParseArgv()

  def ValgrindCommand(self):
    """Get the valgrind command to run."""
    raise RuntimeError, "Never use Valgrind directly. Always subclass and " \
                        "implement ValgrindCommand() at least"

  def Execute(self):
    ''' Execute the app to be tested after successful instrumentation.
    Full execution command-line provided by subclassers via proc.'''
    logging.info("starting execution...")

    proc = self.ValgrindCommand()
    common.RunSubprocess(proc, self._timeout)

    # Always return true, even if running the subprocess failed. We depend on
    # Analyze to determine if the run was valid. (This behaviour copied from
    # the purify_test.py script.)
    return True

  def Analyze(self):
    # Glob all the files in the "valgrind.tmp" directory
    filenames = glob.glob(self.TMP_DIR + "/valgrind.*")
    analyzer = valgrind_analyze.ValgrindAnalyze(self._source_dir, filenames)
    return analyzer.Report()

  def Cleanup(self):
    # Right now, we can cleanup by deleting our temporary directory. Other
    # cleanup is still a TODO?
    shutil.rmtree(self.TMP_DIR)
    return True

  def RunTestsAndAnalyze(self):
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
    # TODO(erg): We probably want to get a version of valgrind that supports
    # the "--track-origins" option...
    proc = ["valgrind", "--smc-check=all", "--leak-check=full",
            "--num-callers=30"]

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

    proc += ["--log-file=" + self.TMP_DIR + "/valgrind.%p"] + self._args
    return proc


class ValgrindMac(Valgrind):

  """Valgrind on Mac OS X.

  Valgrind on OS X does not support suppressions (yet).
  """

  def __init__(self):
    Valgrind.__init__(self)

  def ValgrindCommand(self):
    """Get the valgrind command to run."""
    proc = ["valgrind", "--leak-check=full"]
    # TODO(nirnimesh): Enable --log-file when Analyze() is implemented
    #proc += ["--log-file=" + self.TMP_DIR + "/valgrind.%p"]
    proc += self._args
    return proc

  def Analyze(self):
    # TODO(nirnimesh): Implement analysis later. Valgrind on Mac is new so
    # analysis might not be useful until we have stable output from valgring
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
