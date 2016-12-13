#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Platform-specific utility methods shared by several scripts."""

import os
import re
import signal
import subprocess
import sys
import logging

import google.path_utils

# Distinguish the path_utils.py in this dir from google.path_utils.
import path_utils as layout_package_path_utils

# This will be a native path to the directory this file resides in.
# It can either be relative or absolute depending how it's executed.
THISDIR = os.path.dirname(os.path.abspath(__file__))
def PathFromBase(*pathies):
  return google.path_utils.FindUpward(THISDIR, *pathies)

def PathFromBuildResults(*pathies):
  # FIXME(dkegel): use latest or warn if more than one found?
  for dir in ["sconsbuild", "out", "xcodebuild"]:
    try:
      return google.path_utils.FindUpward(THISDIR, dir, *pathies)
    except:
      pass
  raise google.path_utils.PathNotFound("Unable to find %s under any ancestor of %s" % (os.path.join(*pathies), THISDIR))

def IsNonWindowsPlatformTargettingWindowsResults():
  """Returns true iff this platform is targetting Windows baseline, but isn't
  Windows. By default, in path_utils.py:ExpectedFilename, we expect platforms to
  be targetting Mac."""
  return True

def GetTestListPlatformName():
  """Returns the name we use to identify the platform in the layout test
  test list files."""
  return "LINUX"

class PlatformUtility(object):
  def __init__(self, base_dir):
    """Args:
         base_dir: a directory above which third_party/cygwin can be found,
             used to locate the cygpath executable for path conversions.
    """
  LAYOUTTEST_HTTP_DIR = "LayoutTests/http/tests/"
  PENDING_HTTP_DIR    = "pending/http/tests/"

  def GetAbsolutePath(self, path, force=False):
    """Returns an absolute UNIX path."""
    return os.path.abspath(path)

  # TODO(mmoss): would be great to get rid of the duplication with
  # platform_utils_win/mac.py for the next two functions, but inheritance
  # from tools/python/google on the Windows side makes that a bit difficult.

  def _FilenameToUri(self, path, use_http=False, use_ssl=False, port=8000):
    """Convert a Windows style path to a URI.

    Args:
      path: For an http URI, the path relative to the httpd server's
          DocumentRoot; for a file URI, the full path to the file.
      use_http: if True, returns a URI of the form http://127.0.0.1:8000/.
          If False, returns a file:/// URI.
      use_ssl: if True, returns HTTPS URL (https://127.0.0.1:8000/).
          This parameter is ignored if use_http=False.
      port: The port number to append when returning an HTTP URI
    """
    if use_http:
      protocol = 'http'
      if use_ssl:
        protocol = 'https'
      path = path.replace("\\", "/")
      return "%s://127.0.0.1:%s/%s" % (protocol, str(port), path)
    return "file:///" + self.GetAbsolutePath(path)

  def FilenameToUri(self, full_path):
    relative_path = layout_package_path_utils.RelativeTestFilename(full_path)
    port = None
    use_ssl = False

    # LayoutTests/http/tests/ run off port 8000 and ssl/ off 8443
    if relative_path.startswith(self.LAYOUTTEST_HTTP_DIR):
      relative_path = relative_path[len(self.LAYOUTTEST_HTTP_DIR):]
      port = 8000
    # pending/http/tests/ run off port 9000 and ssl/ off 9443
    elif relative_path.startswith(self.PENDING_HTTP_DIR):
      relative_path = relative_path[len(self.PENDING_HTTP_DIR):]
      port = 9000
    # chrome/http/tests run off of port 8081 with the full path
    elif relative_path.find("/http/") >= 0:
      print relative_path
      port = 8081

    # We want to run off of the http server
    if port:
      if relative_path.startswith("ssl/"):
        port += 443
        use_ssl = True
      return PlatformUtility._FilenameToUri(self,
                                            relative_path,
                                            use_http=True,
                                            use_ssl=use_ssl,
                                            port=port)

    # Run off file://
    return PlatformUtility._FilenameToUri(self, full_path, use_http=False,
                                          use_ssl=False, port=0)

  def KillAllTestShells(self):
    """Kills all instances of the test_shell binary currently running."""
    subprocess.Popen(('killall', '-TERM', self.TestShellBinary()),
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE).wait()

  def MissingLigHTTPd(self):
    print 'Please install using: "sudo apt-get install lighttpd php5-cgi"'
    print 'For complete Linux build requirements, please see:'
    print 'http://code.google.com/p/chromium/wiki/LinuxBuildInstructions'
    sys.exit(1)

  def LigHTTPdExecutablePath(self):
    """Returns the executable path to start LigHTTPd"""
    binpath = "/usr/sbin/lighttpd"
    if os.path.exists(binpath):
      return binpath
    print "Unable to find LigHTTPd executable %s" % binpath
    self.MissingLigHTTPd()

  def LigHTTPdModulePath(self):
    """Returns the library module path for LigHTTPd"""
    modpath = "/usr/lib/lighttpd"
    if os.path.exists(modpath):
      return modpath
    print "Unable to find LigHTTPd modules %s" % modpath
    self.MissingLigHTTPd()

  def LigHTTPdPHPPath(self):
    """Returns the PHP executable path for LigHTTPd"""
    binpath = "/usr/bin/php-cgi"
    if os.path.exists(binpath):
      return binpath
    print "Unable to find PHP CGI executable %s" % binpath
    self.MissingLigHTTPd()

  def ShutDownHTTPServer(self, server_process):
    """Shut down the lighttpd web server. Blocks until it's fully shut down.

    Args:
      server_process: The subprocess object representing the running server
    """
    # server_process is not set when "http_server.py stop" is run manually.
    if server_process is None:
      # TODO(mmoss) This isn't ideal, since it could conflict with lighttpd
      # processes not started by http_server.py, but good enough for now.
      subprocess.call(['killall', '-u', os.getenv('USER'), '-TERM', 'lighttpd'])
    else:
      os.kill(server_process.pid, signal.SIGTERM)

  def WDiffExecutablePath(self):
    """Path to the WDiff executable, which we assume is already installed and
       in the user's $PATH.
    """
    return 'wdiff'

  def ImageCompareExecutablePath(self, target):
    """Path to the image_diff binary."""
    return PathFromBuildResults(target, 'image_diff')

  def TestShellBinary(self):
    """The name of the binary for TestShell."""
    return 'test_shell'

  def TestShellBinaryPath(self, target):
    """Return the platform-specific binary path for our TestShell.

    Args:
      target: Build target mode (debug or release)
    """

    if target in ('Debug', 'Release'):
      try:
        debug_path = PathFromBuildResults('Debug', self.TestShellBinary())
        release_path = PathFromBuildResults('Release', \
                                    self.TestShellBinary())

        debug_mtime = os.stat(debug_path).st_mtime
        release_mtime = os.stat(release_path).st_mtime

        if debug_mtime > release_mtime and target == 'Release' or \
           release_mtime > debug_mtime and target == 'Debug':
          logging.info('\x1b[31mWarning: you are not running the most ' + \
                       'recent test_shell binary. You need to pass ' + \
                       '--debug or not to select between Debug and ' + \
                       'Release.\x1b[0m')
      # This will fail if we don't have both a debug and release binary.
      # That's fine because, in this case, we must already be running the
      # most up-to-date one.
      except OSError:
        pass
      except google.path_utils.PathNotFound:
        pass

    return PathFromBuildResults(target, self.TestShellBinary())

  def FuzzyMatchBinaryPath(self):
    """Return the path to the fuzzy matcher binary."""
    return PathFromBase('third_party', 'fuzzymatch', 'fuzzymatch')

  def TestListPlatformDir(self):
    """Return the platform-specific directory for where the test lists live"""
    return 'linux'

  def PlatformDir(self):
    """Returns the most specific directory name where platform-specific
    results live.
    """
    return 'chromium-linux'

  def PlatformNewResultsDir(self):
    """Returns the directory name in which to output newly baselined tests.
    """
    return self.PlatformDir()
