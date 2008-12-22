#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# common.py

""" Common code used by purify_test.py and quantify_test.py in order to automate
running of Rational Purify and Quantify in a consistent manner.
"""

# Purify and Quantify have a front-end (e.g. quantifyw.exe) which talks to a
# back-end engine (e.g. quantifye.exe).  The back-end seems to handle 
# instrumentation, while the front-end controls program execution and 
# measurement.  The front-end will dynamically launch the back-end if
# instrumentation is needed (sometimes in the middle of a run if a dll is 
# loaded dynamically).
# In an ideal world, this script would simply execute the front-end and check
# the output.  However, purify is not the most reliable or well-documented app
# on the planet, and my attempts to get it to run this way led to the back-end
# engine hanging during instrumentation.  The workaround to this was to run two
# passes, first running the engine to do instrumentation rather than letting 
# the front-end do it for you, then running the front-end to actually do the 
# run.  Each time through we're deleting all of the instrumented files in the
# cache to ensure that we're testing that instrumentation works from scratch.
# (although this can be changed with an option)

import datetime
import logging
import optparse
import os
import subprocess
import sys
import tempfile
import time

import google.logging_utils

# hard-coded location of Rational files and directories
RATIONAL_PATH = os.path.join("C:\\", "Program Files", "Rational")
COMMON_PATH = os.path.join(RATIONAL_PATH, "common")
PPLUS_PATH = os.path.join(RATIONAL_PATH, "PurifyPlus")
PURIFY_PATH = os.path.join(COMMON_PATH, "purify.exe")
PURIFYW_PATH = os.path.join(PPLUS_PATH, "purifyW.exe")
PURIFYE_PATH = os.path.join(PPLUS_PATH, "purifye.exe")
QUANTIFYE_PATH = os.path.join(PPLUS_PATH, "quantifye.exe")
QUANTIFYW_PATH = os.path.join(PPLUS_PATH, "quantifyw.exe")

class TimeoutError(Exception): pass


def _print_line(line, flush=True):
  # Printing to a text file (including stdout) on Windows always winds up
  # using \r\n automatically.  On buildbot, this winds up being read by a master
  # running on Linux, so this is a pain.  Unfortunately, it doesn't matter what
  # we do here, so just leave this comment for future reference.
  print line.rstrip() + '\n',
  if flush:
    sys.stdout.flush()

def RunSubprocess(proc, timeout=0, detach=False):
  """ Runs a subprocess, polling every .2 seconds until it finishes or until
  timeout is reached.  Then kills the process with taskkill.  A timeout <= 0
  means no timeout.
  
  Args:
    proc: list of process components (exe + args)
    timeout: how long to wait before killing, <= 0 means wait forever
    detach: Whether to pass the DETACHED_PROCESS argument to CreateProcess
        on Windows.  This is used by Purify subprocesses on buildbot which
        seem to get confused by the parent console that buildbot sets up.
  """
  logging.info("running %s" % (" ".join(proc)))
  if detach:
    # see MSDN docs for "Process Creation Flags"
    DETACHED_PROCESS = 0x8
    p = subprocess.Popen(proc, creationflags=DETACHED_PROCESS)
  else:
    # For non-detached processes, manually read and print out stdout and stderr.
    # By default, the subprocess is supposed to inherit these from its parent,
    # however when run under buildbot, it seems unable to read data from a
    # grandchild process, so we have to read the child and print the data as if
    # it came from us for buildbot to read it.  We're not sure why this is
    # necessary.
    # TODO(erikkay): should we buffer stderr and stdout separately?
    p = subprocess.Popen(proc, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  if timeout <= 0:
    while p.poll() is None:
      if not detach:
        line = p.stdout.readline()
        while line:
          _print_line(line)
          line = p.stdout.readline()
      time.sleep(0.2)
  else:
    wait_until = time.time() + timeout
    while p.poll() is None and time.time() < wait_until:
      if not detach:
        line = p.stdout.readline()
        while line:
          _print_line(line)
          line = p.stdout.readline()
      time.sleep(0.2)
  if not detach:
    for line in p.stdout.readlines():
      _print_line(line, False)
    p.stdout.flush()
  result = p.poll()
  if result is None:
    subprocess.call(["taskkill", "/T", "/F", "/PID", str(p.pid)])
    logging.error("KILLED %d" % p.pid)
    # give the process a chance to actually die before continuing
    # so that cleanup can happen safely
    time.sleep(1.0)
    logging.error("TIMEOUT waiting for %s" % proc[0])
    raise TimeoutError(proc[0])
  if result:
    logging.error("%s exited with non-zero result code %d" % (proc[0], result))
  return result


def FixPath(path):
  """We pass computed paths to Rational as arguments, so these paths must be
  valid windows paths.  When running in cygwin's python, computed paths
  wind up looking like /cygdrive/c/..., so we need to call out to cygpath
  to fix them up.
  """
  if sys.platform != "cygwin":
    return path
  p = subprocess.Popen(["cygpath", "-a", "-m", path], stdout=subprocess.PIPE)
  return p.communicate()[0].rstrip()


class Rational(object):
  ''' Common superclass for Purify and Quantify automation objects.  Handles
  common argument parsing as well as the general program flow of Instrument,
  Execute, Analyze.
  '''
  
  def __init__(self):
    google.logging_utils.config_root()
    self._out_file = None

  def Run(self):
    '''Call this to run through the whole process: 
    Setup, Instrument, Execute, Analyze'''
    start = datetime.datetime.now()
    retcode = -1
    if self.Setup():
      retcode = self._Run()
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

  def _Run(self):
    retcode = -1
    if not self.Instrument():
      logging.error("Instrumentation failed.")
      return retcode
    if self._instrument_only:
      logging.info("Instrumentation completed successfully.")
      return 0
    if not self.Execute():
      logging.error("Execute failed.")
      return
    retcode = self.Analyze()
    if retcode:
      logging.error("Analyze failed.")
      return retcode
    logging.info("Instrumentation and execution completed successfully.")
    return 0

  def CreateOptionParser(self):
    '''Creates OptionParser with shared arguments.  Overridden by subclassers
    to add custom arguments.'''
    parser = optparse.OptionParser("usage: %prog [options] <program to test>")
    # since the trailing program likely has command-line args of itself
    # we need to stop parsing when we reach the first positional arg
    parser.disable_interspersed_args()
    parser.add_option("-o", "--out_file", dest="out_file", metavar="OUTFILE",
                      default="",
                      help="output data is written to OUTFILE")
    parser.add_option("-s", "--save_cache", 
                      dest="save_cache", action="store_true", default=False,
                      help="don't delete instrumentation cache")
    parser.add_option("-c", "--cache_dir", dest="cache_dir", metavar="CACHEDIR",
                      default="",
                      help="location of instrumentation cache is CACHEDIR")
    parser.add_option("-m", "--manual",
                      dest="manual_run", action="store_true", default=False,
                      help="target app is being run manually, don't timeout")
    parser.add_option("-t", "--timeout",
                      dest="timeout", metavar="TIMEOUT", default=10000,
                      help="timeout in seconds for the run (default 10000)")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="verbose output - enable debug log messages")
    parser.add_option("", "--instrument_only", action="store_true",
                      default=False,
                      help="Only instrument the target without running")
    self._parser = parser

  def Setup(self):
    if self.ParseArgv():
      logging.info("instrumentation cache in %s" % self._cache_dir)
      logging.info("output saving to %s" % self._out_file)
      # Ensure that Rational's common dir and cache dir are in the front of the 
      # path.  The common dir is required for purify to run in any case, and
      # the cache_dir is required when using the /Replace=yes option.
      os.environ["PATH"] = (COMMON_PATH + ";" + self._cache_dir + ";" + 
          os.environ["PATH"])
      # clear the cache to make sure we're starting clean
      self.__ClearInstrumentationCache()
      return True
    return False

  def Instrument(self, proc):
    '''Instrument the app to be tested.  Full instrumentation command-line
    provided by subclassers via proc.'''
    logging.info("starting instrumentation...")
    if RunSubprocess(proc, self._timeout, detach=True) == 0:
      if "/Replace=yes" in proc:
        if os.path.exists(self._exe + ".Original"):
          return True
      elif self._instrument_only:
        # TODO(paulg): Catch instrumentation errors and clean up properly.
        return True
      elif os.path.isdir(self._cache_dir):
        for cfile in os.listdir(self._cache_dir):
          # TODO(erikkay): look for the actual munged purify filename
          ext = os.path.splitext(cfile)[1]
          if ext == ".exe":
            return True
      logging.error("no instrumentation data generated")
    return False

  def Execute(self, proc):
    ''' Execute the app to be tested after successful instrumentation.  
    Full execution command-line provided by subclassers via proc.'''
    logging.info("starting execution...")
    # note that self._args begins with the exe to be run
    proc += self._args
    if RunSubprocess(proc, self._timeout) == 0:
      return True
    return False

  def Analyze(self):
    '''Analyze step after a successful Execution.  Should be overridden
    by the subclasser if instrumentation is desired.
    Returns 0 for success, -88 for warning (see ReturnCodeCommand) and anything
    else for error
    '''
    return -1

  def ParseArgv(self):
    '''Parses arguments according to CreateOptionParser
    Subclassers must override if they have extra arguments.'''
    self.CreateOptionParser()
    self._options, self._args = self._parser.parse_args()
    if self._options.verbose:
      google.logging_utils.config_root(logging.DEBUG)
    self._save_cache = self._options.save_cache
    self._manual_run = self._options.manual_run
    if self._manual_run:
      logging.info("manual run - timeout disabled")
      self._timeout = 0
    else:
      self._timeout = int(self._options.timeout)
      logging.info("timeout set to %ds" % (self._timeout))
    if self._save_cache:
      logging.info("saving instrumentation cache")
    if not self._options.cache_dir:
      try:
        temp_dir = os.environ["TEMP"]
      except KeyError:
        temp_dir = tempfile.mkdtemp()
      self._cache_dir = os.path.join(FixPath(temp_dir),
                                     "instrumentation_cache")
    else:
      self._cache_dir = FixPath(os.path.abspath(self._options.cache_dir))
    if self._options.out_file:
      self._out_file = FixPath(os.path.abspath(self._options.out_file))
    if len(self._args) == 0:
      self._parser.error("missing program to %s" % (self.__class__.__name__,))
      return False
    self._exe = self._args[0]
    self._exe_dir = FixPath(os.path.abspath(os.path.dirname(self._exe)))
    self._instrument_only = self._options.instrument_only
    return True

  def Cleanup(self):
    # delete the cache to avoid filling up the hard drive when we're using
    # temporary directory names
    self.__ClearInstrumentationCache()

  def __ClearInstrumentationCache(self):
    if not self._save_cache:
      logging.info("clearing instrumentation cache %s" % self._cache_dir)
      if os.path.isdir(self._cache_dir):
        for cfile in os.listdir(self._cache_dir):
          file = os.path.join(self._cache_dir, cfile);
          if os.path.isfile(file):
            try:
              os.remove(file)
            except:
              logging.warning("unable to delete file %s: %s" % (file, 
                              sys.exc_info()[0]))



