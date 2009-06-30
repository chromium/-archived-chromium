#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to help start/stop the lighttpd server used by layout tests."""


import logging
import optparse
import os
import platform_utils
import subprocess
import sys
import tempfile
import time
import urllib

import google.path_utils

# This will be a native path to the directory this file resides in.
# It can either be relative or absolute depending how it's executed.
THISDIR = os.path.dirname(os.path.abspath(__file__))

def PathFromBase(*pathies):
  return google.path_utils.FindUpward(THISDIR, *pathies)

class HttpdNotStarted(Exception):
  pass

class Lighttpd:
  # Webkit tests
  try:
    _webkit_tests = PathFromBase('webkit', 'data', 'layout_tests',
                                 'LayoutTests', 'http', 'tests')
  except google.path_utils.PathNotFound:
    # If webkit/data/layout_tests/LayoutTests/http/tests does not exist, assume
    # wekit tests are located in third_party/WebKit/LayoutTests/http/tests.
    try:
      _webkit_tests = PathFromBase('third_party', 'WebKit',
                                   'LayoutTests', 'http', 'tests')
    except google.path_utils.PathNotFound:
      _webkit_tests = None

  # New tests for Chrome
  try:
    _pending_tests = PathFromBase('webkit', 'data', 'layout_tests',
                                  'pending', 'http', 'tests')
  except google.path_utils.PathNotFound:
    _pending_tests = None

  # Path where we can access all of the tests
  _all_tests = PathFromBase('webkit', 'data', 'layout_tests')
  # Self generated certificate for SSL server (for client cert get
  # <base-path>\chrome\test\data\ssl\certs\root_ca_cert.crt)
  _pem_file = PathFromBase('tools', 'python', 'google', 'httpd_config',
                           'httpd2.pem')
  VIRTUALCONFIG = [
    # One mapping where we can get to everything
    {'port': 8081, 'docroot': _all_tests}
  ]

  if _webkit_tests:
    VIRTUALCONFIG.extend(
      # Three mappings (one with SSL enabled) for LayoutTests http tests
      [{'port': 8000, 'docroot': _webkit_tests},
       {'port': 8080, 'docroot': _webkit_tests},
       {'port': 8443, 'docroot': _webkit_tests, 'sslcert': _pem_file}]
    )
  if _pending_tests:
    VIRTUALCONFIG.extend(
      # Three similar mappings (one with SSL enabled) for pending http tests
      [{'port': 9000, 'docroot': _pending_tests},
       {'port': 9080, 'docroot': _pending_tests},
       {'port': 9443, 'docroot': _pending_tests, 'sslcert': _pem_file}]
    )


  def __init__(self, output_dir, background=False, port=None, root=None,
               register_cygwin=None):
    """Args:
      output_dir: the absolute path to the layout test result directory
    """
    self._output_dir = output_dir
    self._process = None
    self._port = port
    self._root = root
    self._register_cygwin = register_cygwin
    if self._port:
      self._port = int(self._port)

  def IsRunning(self):
    return self._process != None

  def Start(self):
    if self.IsRunning():
      raise 'Lighttpd already running'

    base_conf_file = os.path.join(THISDIR, 'lighttpd.conf')
    out_conf_file = os.path.join(self._output_dir, 'lighttpd.conf')
    time_str = time.strftime("%d%b%Y-%H%M%S")
    access_file_name = "access.log-" + time_str + ".txt"
    access_log = os.path.join(self._output_dir, access_file_name)
    log_file_name = "error.log-" + time_str + ".txt"
    error_log = os.path.join(self._output_dir, log_file_name)

    # Write out the config
    f = file(base_conf_file, 'rb')
    base_conf = f.read()
    f.close()

    f = file(out_conf_file, 'wb')
    f.write(base_conf)

    # Write out our cgi handlers.  Run perl through env so that it processes
    # the #! line and runs perl with the proper command line arguments.
    # Emulate apache's mod_asis with a cat cgi handler.
    platform_util = platform_utils.PlatformUtility('')
    f.write(('cgi.assign = ( ".cgi"  => "/usr/bin/env",\n'
             '               ".pl"   => "/usr/bin/env",\n'
             '               ".asis" => "/bin/cat",\n'
             '               ".php"  => "%s" )\n\n') %
                                 platform_util.LigHTTPdPHPPath())

    # Setup log files
    f.write(('server.errorlog = "%s"\n'
             'accesslog.filename = "%s"\n\n') % (error_log, access_log))

    # Setup upload folders. Upload folder is to hold temporary upload files
    # and also POST data. This is used to support XHR layout tests that does
    # POST.
    f.write(('server.upload-dirs = ( "%s" )\n\n') % (self._output_dir))

    # dump out of virtual host config at the bottom.
    if self._root:
      if self._port:
        # Have both port and root dir.
        mappings = [{'port': self._port, 'docroot': self._root}]
      else:
        # Have only a root dir - set the ports as for LayoutTests.
        # This is used in ui_tests to run http tests against a browser.
        mappings = [
          # default set of ports as for LayoutTests but with a specified root.
          {'port': 8000, 'docroot': self._root},
          {'port': 8080, 'docroot': self._root},
          {'port': 8443, 'docroot': self._root, 'sslcert': Lighttpd._pem_file}
        ]
    else:
      mappings = self.VIRTUALCONFIG
    for mapping in mappings:
      ssl_setup = ''
      if 'sslcert' in mapping:
        ssl_setup = ('  ssl.engine = "enable"\n'
                     '  ssl.pemfile = "%s"\n' % mapping['sslcert'])

      f.write(('$SERVER["socket"] == "127.0.0.1:%d" {\n'
               '  server.document-root = "%s"\n' +
               ssl_setup +
               '}\n\n') % (mapping['port'], mapping['docroot']))
    f.close()

    executable = platform_util.LigHTTPdExecutablePath()
    module_path = platform_util.LigHTTPdModulePath()
    start_cmd = [ executable,
                  # Newly written config file
                  '-f', PathFromBase(self._output_dir, 'lighttpd.conf'),
                  # Where it can find its module dynamic libraries
                  '-m', module_path,
                  # Don't background
                  '-D' ]

    # Put the cygwin directory first in the path to find cygwin1.dll
    env = os.environ
    if sys.platform in ('cygwin', 'win32'):
      env['PATH'] = '%s;%s' % (
          PathFromBase('third_party', 'cygwin', 'bin'), env['PATH'])

    if sys.platform == 'win32' and self._register_cygwin:
      setup_mount = PathFromBase('third_party', 'cygwin', 'setup_mount.bat')
      subprocess.Popen(setup_mount).wait()

    logging.info('Starting http server')
    self._process = subprocess.Popen(start_cmd, env=env)

    # Wait for server to start.
    time.sleep(3)

    # Ensure that the server is running on all the desired ports.
    for mapping in mappings:
      url = 'http%s://127.0.0.1:%d/' % ('sslcert' in mapping and 's' or '',
                                        mapping['port'])
      if not self._UrlIsAlive(url):
        raise HttpdNotStarted('Failed to start httpd on port %s' %
                              str(mapping['port']))

    # Our process terminated already
    if self._process.returncode != None:
      raise HttpdNotStarted('Failed to start httpd.')

  def _UrlIsAlive(self, url):
    """Checks to see if we get an http response from |url|.
    We poll the url 5 times with a 3 second delay.  If we don't
    get a reply in that time, we give up and assume the httpd
    didn't start properly.

    Args:
      url: The URL to check.
    Return:
      True if the url is alive.
    """
    attempts = 5
    while attempts > 0:
      try:
        response = urllib.urlopen(url)
        # Server is up and responding.
        return True
      except IOError:
        pass
      attempts -= 1
      # Wait 3 seconds and try again.
      time.sleep(3)

    return False

  # TODO(deanm): Find a nicer way to shutdown cleanly.  Our log files are
  # probably not being flushed, etc... why doesn't our python have os.kill ?
  def Stop(self, force=False):
    if not force and not self.IsRunning():
      return

    logging.info('Shutting down http server')
    platform_util = platform_utils.PlatformUtility('')
    platform_util.ShutDownHTTPServer(self._process)

    if self._process:
      self._process.wait()
      self._process = None

    # Wait a bit to make sure the ports are free'd up
    time.sleep(2)


if '__main__' == __name__:
  # Provide some command line params for starting/stopping the http server
  # manually. Also used in ui_tests to run http layout tests in a browser.
  option_parser = optparse.OptionParser()
  option_parser.add_option('-k', '--server', help='Server action (start|stop)')
  option_parser.add_option('-p', '--port',
      help='Port to listen on (overrides layout test ports)')
  option_parser.add_option('-r', '--root',
      help='Absolute path to DocumentRoot (overrides layout test roots)')
  option_parser.add_option('--register_cygwin', action="store_true",
      dest="register_cygwin", help='Register Cygwin paths (on Win try bots)')
  options, args = option_parser.parse_args()

  if not options.server:
    print 'Usage: %s --server {start|stop} [--root=root_dir]'
    print ' [--port=port_number]' % sys.argv[0]
  else:
    if (options.root is None) and (options.port is not None):
      # specifying root but not port means we want httpd on default set of
      # ports that LayoutTest use, but pointing to a different source of tests.
      # Specifying port but no root does not seem meaningful.
      raise 'Specifying port requires also a root.'
    httpd = Lighttpd(tempfile.gettempdir(),
                     port=options.port,
                     root=options.root,
                     register_cygwin=options.register_cygwin)
    if 'start' == options.server:
      httpd.Start()
    else:
      httpd.Stop(force=True)
