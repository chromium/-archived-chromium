#!/usr/bin/python

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: action_useragentstylesheets.py OUTPUTS -- INPUTS
#
# Multiple OUTPUTS and INPUTS may be listed.  The sections are separated by
# -- arguments.
#
# OUTPUTS must contain two items, in order: a path to UserAgentStyleSheets.h
# and a path to UserAgentStyleSheetsData.cpp.
#
# INPUTS must contain at least two items.  The first item must be the path to
# make-css-file-arrays.pl.  The remaining items are paths to style sheets to
# be fed to that script.


import os
import subprocess
import sys


def SplitArgsIntoSections(args):
  sections = []
  while len(args) > 0:
    if not '--' in args:
      # If there is no '--' left, everything remaining is an entire section.
      dashes = len(args)
    else:
      dashes = args.index('--')

    sections.append(args[:dashes])

    # Next time through the loop, look at everything after this '--'.
    if dashes + 1 == len(args):
      # If the '--' is at the end of the list, we won't come back through the
      # loop again.  Add an empty section now corresponding to the nothingness
      # following the final '--'.
      args = []
      sections.append(args)
    else:
      args = args[dashes + 1:]

  return sections


def main(args):
  sections = SplitArgsIntoSections(args[1:])
  assert len(sections) == 2
  (outputs, inputs) = sections

  assert len(outputs) == 2
  output_h = outputs[0]
  output_cpp = outputs[1]

  make_css_file_arrays = inputs[0]
  style_sheets = inputs[1:]

  # Build up the command.
  command = ['perl', make_css_file_arrays, output_h, output_cpp]
  command.extend(style_sheets)

  # Do it.  check_call is new in 2.5, so simulate its behavior with call and
  # assert.
  return_code = subprocess.call(command)
  assert return_code == 0

  return return_code


if __name__ == '__main__':
  sys.exit(main(sys.argv))
