#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to extract a build, executed by a buildbot slave.
"""

import optparse
import os
import shutil
import sys

import chromium_utils

def main(options, args):
  """ extract build\full-build-win32.zip to build\BuildDir\full-build-win32
      and rename it to build\BuildDir\Target
  """
  project_dir = os.path.abspath(options.build_dir)
  build_dir = os.path.join(project_dir, options.target)
  output_dir = os.path.join(project_dir, 'full-build-win32')

  chromium_utils.ExtractZip('full-build-win32.zip', project_dir)
  chromium_utils.RemoveDirectory(build_dir)
  shutil.move(output_dir, build_dir)
  return 0

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--target', default='Release',
      help='build target to archive (Debug or Release)')
  option_parser.add_option('', '--build-dir', default='chrome',
                           help='path to main build directory (the parent of '
                                'the Release or Debug directory)')

  options, args = option_parser.parse_args()
  sys.exit(main(options, args))
