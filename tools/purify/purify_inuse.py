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

