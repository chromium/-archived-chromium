#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# purify_inuse.py

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


class PurifyInUse(common.Rational):
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
    self._parser.add_option("", "--byte_filter", default=16384,
                            help="prune the tree below this number of bytes")

  def ParseArgv(self):
    if common.Rational.ParseArgv(self):
      self._name = self._options.name
      if not self._name:
        self._name = os.path.basename(self._exe)
      # _out_file can be set in common.Rational.ParseArgv
      if not self._out_file:
        self._out_file = os.path.join(self._latest_dir, "%s.txt" % (self._name))
      self._source_dir = self._options.source_dir
      self._byte_filter = int(self._options.byte_filter)
      return True
    return False

  def _PurifyCommand(self):
    cmd = [common.PURIFYW_PATH, "/CacheDir=" + self._cache_dir,
           "/ShowInstrumentationProgress=no", "/ShowLoadLibraryProgress=no",
           "/AllocCallStackLength=30", "/ErrorCallStackLength=30",
           "/LeaksAtExit=no", "/InUseAtExit=yes"]
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
    return common.Rational.Execute(self, cmd)

  def Analyze(self):
    if not os.path.isfile(self._out_file):
      logging.info("no output file %s" % self._out_file)
      return -1
    pa = purify_analyze.PurifyAnalyze(self._out_file, False,
                                      self._name, self._source_dir)
    if not pa.ReadFile():
      logging.warning("inuse summary suspect due to fatal error during run")
    pa.PrintMemoryInUse(byte_filter=self._byte_filter)
    return 0

if __name__ == "__main__":
  rational = PurifyInUse()
  if rational.Run():
    retcode = 0
  else:
    retcode = -1
  sys.exit(retcode)


