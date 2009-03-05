# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Platform specific utility methods.  This file contains methods that are
specific to running the layout tests on windows.

This file constitutes a complete wrapper for google.platform_utils_win,
implementing or mapping all needed functions from there.  Layout-test scripts
should be able to import only this file (via platform_utils.py), with no need
to fall back to the base functions.
"""

import os
import re
import subprocess

import google.httpd_utils
import google.path_utils
import google.platform_utils_win

# Distinguish the path_utils.py in this dir from google.path_utils.
import path_utils as layout_package_path_utils

# This will be a native path to the directory this file resides in.
# It can either be relative or absolute depending how it's executed.
THISDIR = os.path.dirname(os.path.abspath(__file__))
def PathFromBase(*pathies):
  return google.path_utils.FindUpward(THISDIR, *pathies)

def IsNonWindowsPlatformTargettingWindowsResults():
  """Returns true iff this platform is targetting Windows baseline, but isn't
  Windows. By default, in path_utils.py:ExpectedFilename, we expect platforms to
  be targetting Mac."""
  return False

def GetTestListPlatformName():
  """Returns the name we use to identify the platform in the layout test
  test list files."""
  return "WIN"

class PlatformUtility(google.platform_utils_win.PlatformUtility):
  """Overrides base PlatformUtility methods as needed for layout tests."""

  LAYOUTTEST_HTTP_DIR = "LayoutTests/http/tests/"
  PENDING_HTTP_DIR    = "pending/http/tests/"

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
      return google.platform_utils_win.PlatformUtility.FilenameToUri(self,
                                                       relative_path,
                                                       use_http=True,
                                                       use_ssl=use_ssl,
                                                       port=port)

    # Run off file://
    return google.platform_utils_win.PlatformUtility.FilenameToUri(
        self, full_path)

  def KillAllTestShells(self):
    """Kills all instances of the test_shell binary currently running."""
    subprocess.Popen(['taskkill.exe', '/f', '/im', self.TestShellBinary()],
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE).wait()

  def _GetVirtualHostConfig(self, document_root, port, ssl=False):
    """Returns a <VirtualHost> directive block for an httpd.conf file.  It will
    listen to 127.0.0.1 on each of the given port.
    """
    cygwin_document_root = google.platform_utils_win.GetCygwinPath(
                           document_root)

    return '\n'.join(('<VirtualHost 127.0.0.1:%s>' % port,
                      'DocumentRoot %s' % cygwin_document_root,
                      ssl and 'SSLEngine On' or '',
                      '</VirtualHost>', ''))

  def GetStartHttpdCommand(self, output_dir, apache2=False):
    """Prepares the config file and output directory to start an httpd server.
    Returns a list of strings containing the server's command line+args.

    Creates the test output directory and generates an httpd.conf (or
    httpd2.conf for Apache 2 if apache2 is True) file in it that contains
    the necessary <VirtualHost> directives for running all the http tests.

    WebKit http tests expect the DocumentRoot to be in LayoutTests/http/tests/,
    but that prevents us from running http tests in chrome/ or pending/.  So we
    run two virtual hosts, one on ports 8000 and 8080 for WebKit, and one on
    port 8081 with a much broader DocumentRoot for everything else.  (Note that
    WebKit http tests that have been modified and are temporarily in pending/
    will still fail, if they expect the DocumentRoot to be located as described
    above.)

    Args:
      output_dir: the path to the test output directory.  It will be created.
      apache2: boolean if true will cause this function to return start
               command for Apache 2.x instead of Apache 1.3.x
    """
    layout_dir = google.platform_utils_win.GetCygwinPath(
        layout_package_path_utils.LayoutDataDir())
    main_document_root = os.path.join(layout_dir, "LayoutTests",
                                      "http", "tests")
    pending_document_root = os.path.join(layout_dir, "pending",
                                         "http", "tests")
    chrome_document_root = layout_dir
    apache_config_dir = google.httpd_utils.ApacheConfigDir(self._base_dir)
    mime_types_path = os.path.join(apache_config_dir, "mime.types")

    conf_file_name = "httpd.conf"
    if apache2:
      conf_file_name = "httpd2.conf"
    # Make the test output directory and place the generated httpd.conf in it.
    orig_httpd_conf_path = os.path.join(apache_config_dir, conf_file_name)

    httpd_conf_path = os.path.join(output_dir, conf_file_name)
    google.path_utils.MaybeMakeDirectory(output_dir)
    httpd_conf = open(orig_httpd_conf_path).read()
    httpd_conf = (httpd_conf +
        self._GetVirtualHostConfig(main_document_root, 8000) +
        self._GetVirtualHostConfig(main_document_root, 8080) +
        self._GetVirtualHostConfig(pending_document_root, 9000) +
        self._GetVirtualHostConfig(pending_document_root, 9080) +
        self._GetVirtualHostConfig(chrome_document_root, 8081))
    if apache2:
      httpd_conf += self._GetVirtualHostConfig(main_document_root, 8443,
                                               ssl=True)
      httpd_conf += self._GetVirtualHostConfig(pending_document_root, 9443,
                                               ssl=True)
    f = open(httpd_conf_path, 'wb')
    f.write(httpd_conf)
    f.close()

    return google.platform_utils_win.PlatformUtility.GetStartHttpdCommand(
                                                     self,
                                                     output_dir,
                                                     httpd_conf_path,
                                                     mime_types_path,
                                                     apache2=apache2)

  def LigHTTPdExecutablePath(self):
    """Returns the executable path to start LigHTTPd"""
    return PathFromBase('third_party', 'lighttpd', 'win', 'LightTPD.exe')

  def LigHTTPdModulePath(self):
    """Returns the library module path for LigHTTPd"""
    return PathFromBase('third_party', 'lighttpd', 'win', 'lib')

  def LigHTTPdPHPPath(self):
    """Returns the PHP executable path for LigHTTPd"""
    return PathFromBase('third_party', 'lighttpd', 'win', 'php5', 'php-cgi.exe')

  def ShutDownHTTPServer(self, server_process):
    """Shut down the lighttpd web server. Blocks until it's fully shut down.

    Args:
      server_process: The subprocess object representing the running server.
          Unused in this implementation of the method.
    """
    subprocess.Popen(('taskkill.exe', '/f', '/im', 'LightTPD.exe'),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE).wait()

  def WDiffExecutablePath(self):
    """Path to the WDiff executable, whose binary is checked in on Win"""
    return PathFromBase('third_party', 'cygwin', 'bin', 'wdiff.exe')

  def ImageCompareExecutablePath(self, target):
    return PathFromBase('chrome', target, 'image_diff.exe')

  def TestShellBinary(self):
    """The name of the binary for TestShell."""
    return 'test_shell.exe'

  def TestShellBinaryPath(self, target):
    """Return the platform-specific binary path for our TestShell.

    Args:
      target: Build target mode (debug or release)
    """
    return PathFromBase('chrome', target, self.TestShellBinary())

  def TestListPlatformDir(self):
    """Return the platform-specific directory for where the test lists live."""
    return 'win'

  def PlatformDir(self):
    """Returns the most specific directory name where platform-specific
    results live.
    """
    return 'chromium-win'
