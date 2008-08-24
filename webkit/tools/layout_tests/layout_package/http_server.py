#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to help start/stop the lighttpd server used by layout tests."""


import logging
import optparse
import os
import subprocess
import sys
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
  _webkit_tests = PathFromBase('webkit', 'data', 'layout_tests',
                               'LayoutTests', 'http', 'tests')
  # New tests for Chrome
  _pending_tests = PathFromBase('webkit', 'data', 'layout_tests',
                                'pending', 'http', 'tests')
  # Path where we can access all of the tests
  _all_tests = PathFromBase('webkit', 'data', 'layout_tests')
  # Self generated certificate for SSL server (for client cert get
  # <base-path>\chrome\test\data\ssl\certs\root_ca_cert.crt)
  _pem_file = PathFromBase('tools', 'python', 'google', 'httpd_config',
                           'httpd2.pem')
  VIRTUALCONFIG = [
    # Three mappings (one with SSL enabled) for LayoutTests http tests
    {'port': 8000, 'docroot': _webkit_tests},
    {'port': 8080, 'docroot': _webkit_tests},
    {'port': 8443, 'docroot': _webkit_tests, 'sslcert': _pem_file},
    # Three similar mappings (one with SSL enabled) for pending http tests
    {'port': 9000, 'docroot': _pending_tests},
    {'port': 9080, 'docroot': _pending_tests},
    {'port': 9443, 'docroot': _pending_tests, 'sslcert': _pem_file},
    # One mapping where we can get to everything
    {'port': 8081, 'docroot': _all_tests}
  ]

  def __init__(self, output_dir, background=False):
    """Args:
      output_dir: the absolute path to the layout test result directory
    """
    self._output_dir = output_dir
    self._process = None

  def IsRunning(self):
    return self._process != None

  def Start(self):
    if self.IsRunning():
      raise 'Lighttpd already running'

    base_conf_file = os.path.join(THISDIR, 'lighttpd.conf')
    out_conf_file = os.path.join(self._output_dir, 'lighttpd.conf')
    access_log = os.path.join(self._output_dir, 'access.log.txt')
    error_log = os.path.join(self._output_dir, 'error.log.txt')

    # Write out the config
    f = file(base_conf_file, 'rb')
    base_conf = f.read()
    f.close()

    f = file(out_conf_file, 'wb')
    f.write(base_conf)

    # Write out our cgi handlers.  Run perl through env so that it processes
    # the #! line and runs perl with the proper command line arguments.
    # Emulate apache's mod_asis with a cat cgi handler.
    f.write(('cgi.assign = ( ".cgi"  => "/usr/bin/env",\n'
             '               ".pl"   => "/usr/bin/env",\n'
             '               ".asis" => "/usr/bin/cat",\n'
             '               ".php"  => "%s" )\n\n') %
            PathFromBase('third_party', 'lighttpd', 'php5', 'php-cgi.exe'))

    # Setup log files
    f.write(('server.errorlog = "%s"\n'
             'accesslog.filename = "%s"\n\n') % (error_log, access_log))

    # dump out of virtual host config at the bottom.
    for mapping in self.VIRTUALCONFIG:
      ssl_setup = ''
      if 'sslcert' in mapping:
        ssl_setup = ('  ssl.engine = "enable"\n'
                     '  ssl.pemfile = "%s"\n' % mapping['sslcert'])

      f.write(('$SERVER["socket"] == "127.0.0.1:%d" {\n'
               '  server.document-root = "%s"\n' +
               ssl_setup +
               '}\n\n') % (mapping['port'], mapping['docroot']))
    f.close()

    start_cmd = [ PathFromBase('third_party', 'lighttpd', 'LightTPD.exe'),
                  # Newly written config file
                  '-f', PathFromBase(self._output_dir, 'lighttpd.conf'),
                  # Where it can find it's module dynamic libraries
                  '-m', PathFromBase('third_party', 'lighttpd', 'lib'),
                  # Don't background
                  '-D' ]

    # Put the cygwin directory first in the path to find cygwin1.dll
    env = os.environ
    env['PATH'] = '%s;%s' % (
        PathFromBase('third_party', 'cygwin', 'bin'), env['PATH'])

    logging.info('Starting http server')
    self._process = subprocess.Popen(start_cmd, env=env)

    # Ensure that the server is running on all the desired ports.
    for mapping in self.VIRTUALCONFIG:
      url = 'http%s://127.0.0.1:%d/' % ('sslcert' in mapping and 's' or '',
                                        mapping['port'])
      if not self._UrlIsAlive(url):
        raise HttpdNotStarted('Failed to start httpd on port %s' % str(port))

    # Our process terminated already
    if self._process.returncode != None:
      raise HttpdNotStarted('Failed to start httpd.')

  def _UrlIsAlive(self, url):
    """Checks to see if we get an http response from |url|.
    We poll the url 5 times with a 1 second delay.  If we don't
    get a reply in that time, we give up and assume the httpd
    didn't start properly.

    Args:
      url: The URL to check.
    Return:
      True if the url is alive.
    """
    wait_time = 5
    while wait_time > 0:
      try:
        response = urllib.urlopen(url)
        # Server is up and responding.
        return True
      except IOError:
        pass
      wait_time -= 1
      # Wait a second and try again.
      time.sleep(1)

    return False

  # TODO(deanm): Find a nicer way to shutdown cleanly.  Our log files are
  # probably not being flushed, etc... why doesn't our python have os.kill ?
  def Stop(self, force=False):
    if not force and not self.IsRunning():
      return

    logging.info('Shutting down http server')

    subprocess.Popen(('taskkill.exe', '/f', '/im', 'LightTPD.exe'),
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE).wait()

    if self._process:
      self._process.wait()
      self._process = None

    # Wait a bit to make sure the ports are free'd up
    time.sleep(2)


if '__main__' == __name__:
  # Provide some command line params for starting/stopping the http server
  # manually.
  option_parser = optparse.OptionParser()
  option_parser.add_option('-k', '--server', help='Server action (start|stop)')
  options, args = option_parser.parse_args()
  
  if not options.server:
    print "Usage: %s --server {start|stop} [--apache2]" % sys.argv[0]
  else:
    httpd = Lighttpd('c:/cygwin/tmp')
  if 'start' == options.server:
    httpd.Start()
  else:
    httpd.Stop(force=True)

