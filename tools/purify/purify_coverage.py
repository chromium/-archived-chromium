#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# purify_coverage.py

import logging
import optparse
import os
import re
import sys

import google.path_utils

# local modules
import common
import purify_analyze
import purify_message


class PurifyCoverage(common.Rational):
  def __init__(self):
    common.Rational.__init__(self)
    script_dir = google.path_utils.ScriptDir()
    self._latest_dir = os.path.join(script_dir, "latest")

  def CreateOptionParser(self):
    common.Rational.CreateOptionParser(self)
    self._parser.description = __doc__
    self._parser.add_option("-n", "--name",
                      dest="name", default=None,
                      help="name of the test being run "
                           "(used for output filenames)")
    self._parser.add_option("", "--source_dir",
                            help="path to top of source tree for this build"
                                 "(used to normalize source paths in baseline)")

  def ParseArgv(self):
    if common.Rational.ParseArgv(self):
      self._name = self._options.name
      if not self._name:
        self._name = os.path.basename(self._exe)
      # _out_file can be set in common.Rational.ParseArgv
      if not self._out_file:
        self._out_file = os.path.join(self._latest_dir,
                                      "%s_coverage.txt" % (self._name))
      self._source_dir = self._options.source_dir
      return True
    return False

  def _PurifyCommand(self):
    cmd = [common.PURIFYW_PATH, "/CacheDir=" + self._cache_dir,
           "/ShowInstrumentationProgress=no", "/ShowLoadLibraryProgress=no",
           "/AllocCallStackLength=30", "/Coverage",
           "/CoverageDefaultInstrumentationType=line"]
    return cmd

  def Instrument(self):
    cmd = self._PurifyCommand()
    # /Run=no means instrument only
    cmd.append("/Run=no")
    cmd.append(os.path.abspath(self._exe))
    return common.Rational.Instrument(self, cmd)

  def Execute(self):
    cmd = self._PurifyCommand()
    cmd.append("/SaveTextData=" + self._out_file)
    # TODO(erikkay): should we also do /SaveMergeTextData?
    return common.Rational.Execute(self, cmd)

  def Analyze(self):
    if not os.path.isfile(self._out_file):
      logging.info("no output file %s" % self._out_file)
      return -1
    # TODO(erikkay): parse the output into a form we could use on the buildbots
    return 0

if __name__ == "__main__":
  rational = PurifyCoverage()
  if rational.Run():
    retcode = 0
  else:
    retcode = -1
  sys.exit(retcode)


