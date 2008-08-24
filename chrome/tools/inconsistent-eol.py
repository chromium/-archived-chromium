#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

