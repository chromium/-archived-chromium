#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

"""Keep a local directory in sync with a website tar file.

This module downloads a tgz, and expands it as needed.
It supports username and password with basic authentication.
"""

import os
import shutil
import tarfile
import http_download


def SyncTgz(url, target, username=None, password=None, verbose=True):
  """Download a file from a remote server.

  Args:
    url: A URL to download from.
    target: Directory to extract to and prefix to use for tgz file.
    username: Optional username for download.
    password: Optional password for download (ignored if no username).
    verbose: Flag indicating if status shut be printed.
  """
  shutil.rmtree(target, True)
  tgz_filename = target + '.tgz'

  if verbose:
    print 'Downloading %s to %s...' % (url, tgz_filename)
  http_download.HttpDownload(url, tgz_filename,
                             username=username, password=password)

  if verbose:
    print 'Extracting from %s...' % tgz_filename
  tgz = tarfile.open(tgz_filename, 'r')
  for m in tgz:
    if verbose:
      print m.name
    tgz.extract(m, target)
  tgz.close()
  os.remove(tgz_filename)

  if verbose:
    print 'Update complete.'
