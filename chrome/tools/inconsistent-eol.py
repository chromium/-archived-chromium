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

"""Find and fix files with inconsistent line endings.

This script requires 'dos2unix.exe' and 'unix2dos.exe' from Cygwin; they
must be in the user's PATH.

Arg: a file containing a list of relative or absolute file paths.  The
    argument passed to this script, as well as the paths in the file, may be
    relative paths or absolute Windows-style paths (with either type of
    slash).  The list might be generated with 'find -type f' or extracted from
    a gvn changebranch listing, for example.
"""

import errno
import logging
import subprocess
import sys


# Whether to produce excessive debugging output for each file in the list.
DEBUGGING = False


class Error(Exception):
  """Local exception class."""
  pass


def CountChars(text, str):
  """Count the number of instances of the given string in the text."""
  split = text.split(str)
  logging.debug(len(split) - 1)
  return len(split) - 1

def PrevailingEOLName(crlf, cr, lf):
  """Describe the most common line ending.

  Args:
    crlf: How many CRLF (\r\n) sequences are in the file.
    cr: How many CR (\r) characters are in the file, excluding CRLF sequences.
    lf: How many LF (\n) characters are in the file, excluding CRLF sequences.

  Returns:
    A string describing the most common of the three line endings.
  """
  most = max(crlf, cr, lf)
  if most == cr:
    return 'cr'
  if most == crlf:
    return 'crlf'
  return 'lf'

def FixEndings(file, crlf, cr, lf):
  """Change the file's line endings to CRLF or LF, whichever is more common."""
  most = max(crlf, cr, lf)
  if most == crlf:
    result = subprocess.call('unix2dos.exe %s' % file, shell=True)
    if result:
      raise Error('Error running unix2dos.exe %s' % file)
  else:
    result = subprocess.call('dos2unix.exe %s' % file, shell=True)
    if result:
      raise Error('Error running dos2unix.exe %s' % file)


def main(argv=None):
  """Process the list of files."""
  if not argv or len(argv) < 2:
    raise Error('No file list given.')

  for filename in open(argv[1], 'r'):
    filename = filename.strip()
    logging.debug(filename)
    try:
      # Open in binary mode to preserve existing line endings.
      text = open(filename, 'rb').read()
    except IOError, e:
      if e.errno != errno.ENOENT:
        raise
      logging.warning('File %s not found.' % filename)
      continue
    crlf = CountChars(text, '\r\n')
    cr = CountChars(text, '\r') - crlf
    lf = CountChars(text, '\n') - crlf

    if ((crlf > 0 and cr > 0) or
        (crlf > 0 and lf > 0) or
        (  lf > 0 and cr > 0)):
      print ('%s: mostly %s' % (filename, PrevailingEOLName(crlf, cr, lf)))
      FixEndings(filename, crlf, cr, lf)

if '__main__' == __name__:
  if DEBUGGING:
    debug_level = logging.DEBUG
  else:
    debug_level = logging.INFO
  logging.basicConfig(level=debug_level,
                      format='%(asctime)s %(levelname)-7s: %(message)s',
                      datefmt='%H:%M:%S')

  sys.exit(main(sys.argv))
