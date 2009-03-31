#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

# Files with these extensions will be considered source files.
SOURCE_FILE_EXTENSIONS = [
    '.c', '.cc', '.cpp', '.h', '.m', '.mm', '.py', '.mk', '.am', '.json',
]
EXCLUDED_PATHS = [
    r"breakpad[\\\/].*",
    r"chrome[\\\/]Debug[\\\/].*",
    r"chrome[\\\/]Hammer[\\\/].*",
    r"chrome[\\\/]Release[\\\/].*",
    r"xcodebuild[\\\/].*",
    r"skia[\\\/].*",
    r".*third_party[\\\/].*",
    r"v8[\\\/].*",
]

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
  # TODO(maruel): max_cols is temporarily disabled. Reenable once the source
  # tree is in better shape.
  return LocalChecks(input_api, output_api, max_cols=0)


def CheckChangeOnCommit(input_api, output_api):
  # TODO(maruel): max_cols is temporarily disabled. Reenable once the source
  # tree is in better shape.
  return (LocalChecks(input_api, output_api, max_cols=0) +
          input_api.canned_checks.CheckDoNotSubmit(input_api, output_api))


def LocalChecks(input_api, output_api, max_cols=80):
  """Reports an error if for any source file in SOURCE_FILE_EXTENSIONS:
     - uses CR (or CRLF)
     - contains a TAB
     - has a line that ends with whitespace
     - contains a line >|max_cols| cols unless |max_cols| is 0.
     - File does not end in a newline, or ends in more than one.

  Note that the whole file is checked, not only the changes.
  """
  C_SOURCE_FILE_EXTENSIONS = ('.c', '.cc', '.cpp', '.h', '.inl')
  cr_files = []
  eof_files = []
  results = []
  excluded_paths = [input_api.re.compile(x) for x in EXCLUDED_PATHS]
  files = input_api.AffectedFiles(include_deletes=False)
  for f in files:
    path = f.LocalPath()
    root, ext = input_api.os_path.splitext(path)
    # Look for unsupported extensions.
    if not ext in SOURCE_FILE_EXTENSIONS:
      continue
    # Look for excluded paths.
    found = False
    for item in excluded_paths:
      if item.match(path):
        found = True
        break
    if found:
      continue

    # Need to read the file ourselves since AffectedFile.NewContents()
    # will normalize line endings.
    contents = _ReadFile(path)
    if '\r' in contents:
      cr_files.append(path)

    # Check that the file ends in one and only one newline character.
    if len(contents) > 0 and (contents[-1:] != "\n" or contents[-2:-1] == "\n"):
      eof_files.append(path)

    local_errors = []
    # Remove EOL character.
    lines = contents.splitlines()
    line_num = 1
    for line in lines:
      if line.endswith(' '):
        local_errors.append(output_api.PresubmitError(
            '%s, line %s ends with whitespaces.' %
            (path, line_num)))
      # Accept lines with http://, https:// and C #define/#pragma/#include to
      # exceed the max_cols rule.
      if (max_cols and
          len(line) > max_cols and
          not 'http://' in line and
          not 'https://' in line and
          not (line[0] == '#' and ext in C_SOURCE_FILE_EXTENSIONS)):
        local_errors.append(output_api.PresubmitError(
            '%s, line %s has %s chars, please reduce to %d chars.' %
            (path, line_num, len(line), max_cols)))
      if '\t' in line:
        local_errors.append(output_api.PresubmitError(
            "%s, line %s contains a tab character." %
            (path, line_num)))
      line_num += 1
      # Just show the first 5 errors.
      if len(local_errors) == 6:
        local_errors.pop()
        local_errors.append(output_api.PresubmitError("... and more."))
        break
    results.extend(local_errors)

  if cr_files:
    results.append(output_api.PresubmitError(
        'Found CR (or CRLF) line ending in these files, please use only LF:',
        items=cr_files))
  if eof_files:
    results.append(output_api.PresubmitError(
        'These files should end in one (and only one) newline character:',
        items=eof_files))
  return results
