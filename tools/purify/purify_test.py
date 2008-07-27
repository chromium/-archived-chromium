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

# purify_test.py

'''Runs an exe  through Purify and verifies that Purify was
able to successfully instrument and run it.  The original purpose was 
to be able to identify when a change to our code breaks our ability to Purify 
the app. This can happen with seemingly innocuous changes to code due to bugs 
in Purify, and is notoriously difficult to track down when it does happen.
Perhaps more importantly in the long run, this can also automate detection of
leaks and other memory bugs.  It also may be useful to allow people to run
Purify in a consistent manner without having to worry about broken PATHs,
corrupt instrumentation, or other per-machine flakiness that Purify is
sometimes subject to.
'''

import glob
import logging
import optparse
import os
import re
import shutil
import sys
import time

import google.path_utils

# local modules
import common
import purify_analyze

class Purify(common.Rational):
  def __init__(self):
    common.Rational.__init__(self)
    self._data_dir = None

  def CreateOptionParser(self):
    common.Rational.CreateOptionParser(self)
    self._parser.description = __doc__
    self._parser.add_option("-e", "--echo_to_stdout",
                      dest="echo_to_stdout", action="store_true", default=False,
                      help="echo purify output to standard output")
    self._parser.add_option("-b", "--baseline",
                      dest="baseline", action="store_true", default=False,
                      help="create baseline error files")
    self._parser.add_option("-n", "--name",
                      dest="name", default=None,
                      help="name of the test being run "
                           "(used for output filenames)")
    self._parser.add_option("", "--source_dir",
                            help="path to top of source tree for this build"
                                 "(used to normalize source paths in baseline)")
    self._parser.add_option("", "--exe",
                            help="The actual exe to instrument which is "
                                 "different than the program being run.  This "
                                 "is useful when the exe you want to purify is "
                                 "run by another script or program.")
    self._parser.add_option("", "--data_dir",
                            help="path to where purify data files live")

  def ParseArgv(self):
    if common.Rational.ParseArgv(self):
      if self._options.exe:
        self._exe = self._options.exe;
        if not os.path.isfile(self._exe):
          logging.error("file doesn't exist " + self._exe)
          return False
        self._exe_dir = common.FixPath(os.path.abspath(os.path.dirname(self._exe)))
      self._echo_to_stdout = self._options.echo_to_stdout
      self._baseline = self._options.baseline
      self._name = self._options.name
      if not self._name:
        self._name = os.path.basename(self._exe)
      # _out_file can be set in common.Rational.ParseArgv
      if not self._out_file:
        self._out_file = os.path.join(self._latest_dir, "%s.txt" % self._name)
      self._source_dir = self._options.source_dir
      self._data_dir = self._options.data_dir
      if not self._data_dir:
        self._data_dir = os.path.join(script_dir, "data")
      return True
    return False

  def _PurifyCommand(self):
    cmd = [common.PURIFY_PATH, "/CacheDir=" + self._cache_dir]
    return cmd

  def Setup(self):
    script_dir = google.path_utils.ScriptDir()
    self._latest_dir = os.path.join(script_dir, "latest")
    if common.Rational.Setup(self):
      pft_file = os.path.join(script_dir, "data", "filters.pft")
      shutil.copyfile(pft_file, self._exe.replace(".exe", "_exe.pft"))
      string_list = [
          "[Purify]",
          "option -cache-dir=\"%s\"" % (self._cache_dir),
          "option -save-text-data=\"%s\"" % (common.FixPath(self._out_file)),
          "option -alloc-call-stack-length=30",
          "option -error-call-stack-length=30",
          "option -free-call-stack-length=30",
          "option -leaks-at-exit=yes",
          "option -in-use-at-exit=no"
          ]
      ini_file = self._exe.replace(".exe", "_pure.ini")
      if os.path.isfile(ini_file):
        ini_file_orig = ini_file + ".Original"
        if not os.path.isfile(ini_file_orig):
          os.rename(ini_file, ini_file_orig)
      try:
        f = open(ini_file, "w+")
        f.write('\n'.join(string_list))
      except IOError, (errno, strerror):
        logging.error("error writing to file %s (%d, %s)" % ini_file, errno,
                      strerror)
        return False
      if f:
        f.close()
      return True
    return False

  def Instrument(self):
    if not os.path.isfile(self._exe):
      logging.error("file doesn't exist " + self._exe)
      return False
    cmd = self._PurifyCommand()
    # /Run=no means instrument only, /Replace=yes means replace the exe in place
    cmd.extend(["/Run=no", "/Replace=yes"])
    cmd.append(os.path.abspath(self._exe))
    return common.Rational.Instrument(self, cmd)

  def _ExistingOutputFiles(self):
    pat_multi =  re.compile('(.*)%[0-9]+d(.*)')
    m = pat_multi.match(self._out_file)
    if m:
      g = m.group(1) + '[0-9]*' + m.group(2)
      out = glob.glob(g)
      if os.path.isfile(m.group(1) + m.group(2)):
        out.append(m.group(1) + m.group(2))
      return out
    if not os.path.isfile(self._out_file):
      return []
    return [self._out_file]

  def Execute(self):
    # delete the old file(s) to make sure that this run actually generated
    # something new
    out_files = self._ExistingOutputFiles()
    for f in out_files:
      os.remove(f)
    common.Rational.Execute(self, [])
    # Unfortunately, when we replace the exe, there's no way here to figure out
    # if purify is actually going to output a file or if the exe just crashed
    # badly.  The reason is that it takes some small amount of time for purify
    # to dump out the file.
    count = 60
    while count > 0 and not os.path.isfile(self._out_file):
      time.sleep(0.2)
      count -= 1
    # Always return true, even if Execute failed - we'll depend on Analyze to
    # determine if the run was valid.
    return True

  def Analyze(self):
    out_files = self._ExistingOutputFiles()
    if not len(out_files):
      logging.info("no output files matching %s" % self._out_file)
      return -1
    pa = purify_analyze.PurifyAnalyze(out_files, self._echo_to_stdout,
                                      self._name, self._source_dir,
                                      self._data_dir)
    if not pa.ReadFile():
      # even though there was a fatal error during Purify, it's still useful
      # to see the normalized output
      pa.PrintSummary()
      if self._baseline:
        logging.warning("baseline not generated due to fatal error")
      else:
        logging.warning("baseline comparison skipped due to fatal error")
      return -1
    if self._baseline:
      pa.PrintSummary(False)
      if pa.SaveResults():
        return 0
      return -1
    else:
      retcode = pa.CompareResults()
      if retcode != 0:
        pa.SaveResults(self._latest_dir)
      pa.PrintSummary()
      # with more than one output file, it's also important to emit the bug
      # report which includes info on the arguments that generated each stack
      if len(out_files) > 1:
        pa.PrintBugReport()
      return retcode

  def Cleanup(self):
    common.Rational.Cleanup(self);
    cmd = self._PurifyCommand()
    # undo the /Replace=yes that was done in Instrument(), which means to 
    # remove the instrumented exe, and then rename exe.Original back to exe.
    cmd.append("/UndoReplace")
    cmd.append(os.path.abspath(self._exe))
    common.RunSubprocess(cmd, self._timeout, detach=True)
    # if we overwrote an existing ini file, restore it
    ini_file = self._exe.replace(".exe", "_pure.ini")
    if os.path.isfile(ini_file):
      os.remove(ini_file)
    ini_file_orig = ini_file + ".Original"
    if os.path.isfile(ini_file_orig):
      os.rename(ini_file_orig, ini_file)
    # remove the pft file we wrote out
    pft_file = self._exe.replace(".exe", "_exe.pft")
    if os.path.isfile(pft_file):
      os.remove(pft_file)


if __name__ == "__main__":
  rational = Purify()
  retcode = rational.Run()
  sys.exit(retcode)

