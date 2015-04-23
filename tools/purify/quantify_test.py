#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# quantify_test.py

'''Runs an app through Quantify and verifies that Quantify was able to
successfully instrument and run it.  The original purpose was to allow people
to run Quantify in a consistent manner without having to worry about broken
PATHs, corrupt instrumentation, or other per-machine flakiness that Quantify is
sometimes subject to.  Unlike purify_test, the output from quantify_test is
a binary file, which is much more useful in manual analysis.  As such, this
tool is not particularly interesting for automated analysis yet.
'''

import os
import sys

# local modules
import common

class Quantify(common.Rational):
  def __init__(self):
    common.Rational.__init__(self)

  def CreateOptionParser(self):
    common.Rational.CreateOptionParser(self)
    self._parser.description = __doc__

  def ParseArgv(self):
    if common.Rational.ParseArgv(self):
      if not self._out_file:
        self._out_file = os.path.join(self._cache_dir,
                                      "%s.qfy" % (os.path.basename(self._exe)))
      return True
    return False

  def Instrument(self):
    proc = [common.QUANTIFYE_PATH, "-quantify",
            '-quantify_home="' + common.PPLUS_PATH + '"' ,
            "/CacheDir=" + self._cache_dir,
            "-first-search-dir=" + self._exe_dir, self._exe]
    return common.Rational.Instrument(self, proc)

  def Execute(self):
    # TODO(erikkay): add an option to also do /SaveTextData and add an
    # Analyze method for automated analysis of that data.
    proc = [common.QUANTIFYW_PATH, "/CacheDir=" + self._cache_dir,
            "/ShowInstrumentationProgress=no", "/ShowLoadLibraryProgress=no",
            "/SaveData=" + self._out_file]
    return common.Rational.Execute(self, proc)

if __name__ == "__main__":
  retcode = -1
  rational = Quantify()
  if rational.Run():
    retcode = 0
  sys.exit(retcode)


