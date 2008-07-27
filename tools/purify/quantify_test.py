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
  
