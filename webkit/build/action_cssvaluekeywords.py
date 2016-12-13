#!/usr/bin/python

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# action_cssvaluekeywords.py is a harness script to connect actions sections of
# gyp-based builds to makevalues.pl.
#
# usage: action_cssvaluekeywords.py OUTPUTS -- INPUTS
#
# Exactly two outputs must be specified: a path to each of CSSValueKeywords.c
# and CSSValueKeywords.h.
#
# Multiple inputs may be specified.  One input must have a basename of
# makevalues.pl; this is taken as the path to makevalues.pl.  All other inputs
# are paths to .in files that are used as input to makevalues.pl; at least
# one, CSSValueKeywords.in, is required.


import os
import posixpath
import shutil
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
  (outputs, inputs) = SplitArgsIntoSections(args[1:])

  # Make all output pathnames absolute so that they can be accessed after
  # changing directory.
  for index in xrange(0, len(outputs)):
    outputs[index] = os.path.abspath(outputs[index])

  output_dir = os.path.dirname(outputs[0])

  # Look at the inputs and figure out which one is makevalues.pl and which are
  # inputs to that script.
  makevalues_input = None
  in_files = []
  for input in inputs:
    # Make input pathnames absolute so they can be accessed after changing
    # directory.  On Windows, convert \ to / for inputs to the perl script to
    # work around the intermix of activepython + cygwin perl.
    input_abs = os.path.abspath(input)
    input_abs_posix = input_abs.replace(os.path.sep, posixpath.sep)
    input_basename = os.path.basename(input)
    if input_basename == 'makevalues.pl':
      assert makevalues_input == None
      makevalues_input = input_abs
    elif input_basename.endswith('.in'):
      in_files.append(input_abs_posix)
    else:
      assert False

  assert makevalues_input != None
  assert len(in_files) >= 1

  # Change to the output directory because makevalues.pl puts output in its
  # working directory.
  os.chdir(output_dir)

  # Merge all in_files into a single file whose name will be the same as the
  # first listed in_file, but in the output directory.
  merged_path = os.path.basename(in_files[0])
  merged = open(merged_path, 'wb')  # 'wb' to get \n only on windows

  # Make sure there aren't any duplicate lines in the in files.  Lowercase
  # everything because CSS values are case-insensitive.
  line_dict = {}
  for in_file_path in in_files:
    in_file = open(in_file_path)
    for line in in_file:
      line = line.rstrip()
      if line.startswith('#'):
        line = ''
      if line == '':
        continue
      line = line.lower()
      if line in line_dict:
        raise KeyError, 'Duplicate value %s' % line
      line_dict[line] = True
      print >>merged, line
    in_file.close()

  merged.close()

  # Build up the command.
  command = ['perl', makevalues_input]

  # Do it.  check_call is new in 2.5, so simulate its behavior with call and
  # assert.
  return_code = subprocess.call(command)
  assert return_code == 0

  # Don't leave behind the merged file or the .gperf file created by
  # makevalues.
  (root, ext) = os.path.splitext(merged_path)
  gperf_path = root + '.gperf'
  os.unlink(gperf_path)
  os.unlink(merged_path)

  # Go through the outputs.  Any output that belongs in a different directory
  # is moved.  Do a copy and delete instead of rename for maximum portability.
  # Note that all paths used in this section are still absolute.
  for output in outputs:
    this_output_dir = os.path.dirname(output)
    if this_output_dir != output_dir:
      output_basename = os.path.basename(output)
      src = os.path.join(output_dir, output_basename)
      dst = os.path.join(this_output_dir, output_basename)
      shutil.copyfile(src, dst)
      os.unlink(src)

  return return_code


if __name__ == '__main__':
  sys.exit(main(sys.argv))
