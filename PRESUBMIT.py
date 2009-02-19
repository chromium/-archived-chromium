#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""


import os


# Files with these extensions will be considered source files
SOURCE_FILE_EXTENSIONS = ['.c', '.cc', '.cpp', '.h', '.m', '.mm', '.py']


def ReadFile(path):
  """Given a path, returns the full contents of the file.
  
  Reads files in binary format.
  """
  fo = open(path, 'rb')
  try:
    contents = fo.read()
  finally:
    fo.close()
  return contents


# Seam for unit testing
_ReadFile = ReadFile


def CheckChangeOnUpload(input_api, output_api):
  return (CheckNoCrOrTabs(input_api, output_api) +
          input_api.canned_checks.CheckDoNotSubmit(input_api, output_api))


def CheckChangeOnCommit(input_api, output_api):
  # No extra checks on commit for now
  return CheckChangeOnUpload(input_api, output_api)


def CheckNoCrOrTabs(input_api, output_api):
  """Reports an error if source files use CR (or CRLF) or TAB.
  """
  cr_files = []
  tab_files = []
  results = []
  
  for f in input_api.AffectedTextFiles(include_deletes=False):
    path = f.LocalPath()
    root, ext = os.path.splitext(path)
    if ext in SOURCE_FILE_EXTENSIONS:
      # Need to read the file ourselves since AffectedFile.NewContents()
      # will normalize line endings.
      contents = _ReadFile(path)
      if '\r' in contents:
        cr_files.append(path)
      if '\t' in contents:
        tab_files.append(path)
  if cr_files:
    results.append(output_api.PresubmitError(
        'Found CR (or CRLF) line ending in these files, please use only LF:',
        items=cr_files))
  if tab_files:
    results.append(output_api.PresubmitError(
        'Found tabs in the following files, please use spaces',
        items=tab_files))
  return results
