#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
lastchange.py -- Chromium revision fetching utility.
"""

import optparse
import os
import re
import subprocess
import sys


def svn_fetch_revision():
  """
  Fetch the Subversion revision for the local tree.

  Errors are swallowed.
  """
  try:
    p = subprocess.Popen(['svn', 'info'],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         shell=(sys.platform=='win32'))
  except OSError, e:
    # 'svn' is apparently either not installed or not executable.
    return None
  revision = None
  if p:
    svn_re = re.compile('^Revision:\s+(\d+)', re.M)
    m = svn_re.search(p.stdout.read())
    if m:
      revision = m.group(1)
  return revision


def git_fetch_id():
  """
  Fetch the GIT identifier for the local tree.

  Errors are swallowed.
  """
  try:
    p = subprocess.Popen(['git', 'log', '-1'],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         shell=(sys.platform=='win32'))
  except OSError:
    # 'git' is apparently either not installed or not executable.
    return None
  id = None
  if p:
    git_re = re.compile('^\s*git-svn-id:\s+(\S+)@(\d+)', re.M)
    m = git_re.search(p.stdout.read())
    if m:
      id = m.group(2)
  return id


def fetch_change():
  """
  Returns the last change, from some appropriate revision control system.
  """
  change = svn_fetch_revision()
  if not change and sys.platform in ('linux2',):
    change = git_fetch_id()
  if not change:
    change = '0'
  return change


def write_if_changed(file_name, contents):
  """
  Writes the specified contents to the specified file_name
  iff the contents are different than the current contents.
  """
  try:
    old_contents = open(file_name, 'r').read()
  except EnvironmentError:
    pass
  else:
    if contents == old_contents:
      return
    os.unlink(file_name)
  open(file_name, 'w').write(contents)


def main(argv=None):
  if argv is None:
    argv = sys.argv

  parser = optparse.OptionParser(usage="lastchange.py [-h] [[-o] FILE]")
  parser.add_option("-o", "--output", metavar="FILE",
                    help="write last change to FILE")
  opts, args = parser.parse_args(argv[1:])

  out_file = opts.output

  while len(args) and out_file is None:
    if out_file is None:
      out_file = args.pop(0)
  if args:
    sys.stderr.write('Unexpected arguments: %r\n\n' % args)
    parser.print_help()
    sys.exit(2)

  change = fetch_change()

  contents = "LASTCHANGE=%s\n" % change

  if out_file:
    write_if_changed(out_file, contents)
  else:
    sys.stdout.write(contents)

  return 0


if __name__ == '__main__':
  sys.exit(main())
