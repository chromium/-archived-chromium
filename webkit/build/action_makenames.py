#!/usr/bin/python

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# action_makenames.py is a harness script to connect actions sections of
# gyp-based builds to make_names.pl.
#
# usage: action_makenames.py OUTPUTS -- INPUTS [-- OPTIONS]
#
# Multiple OUTPUTS, INPUTS, and OPTIONS may be listed.  The sections are
# separated by -- arguments.
#
# The directory name of the first output is chosen as the directory in which
# make_names will run.  If the directory name for any subsequent output is
# different, those files will be moved to the desired directory.
#
# Multiple INPUTS may be listed.  An input with a basename matching
# "make_names.pl" is taken as the path to that script.  Inputs with names
# ending in TagNames.in or tags.in are taken as tag inputs.  Inputs with names
# ending in AttributeNames.in or attrs.in are taken as attribute inputs.  There
# may be at most one tag input and one attribute input.  A make_names.pl input
# is required and at least one tag or attribute input must be present.
#
# OPTIONS is a list of additional options to pass to make_names.pl.  This
# section need not be present.


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
  sections = SplitArgsIntoSections(args[1:])
  assert len(sections) == 2 or len(sections) == 3
  (outputs, inputs) = sections[:2]
  if len(sections) == 3:
    options = sections[2]
  else:
    options = []

  # Make all output pathnames absolute so that they can be accessed after
  # changing directory.
  for index in xrange(0, len(outputs)):
    outputs[index] = os.path.abspath(outputs[index])

  output_dir = os.path.dirname(outputs[0])

  # Look at the inputs and figure out which ones are make_names.pl, tags, and
  # attributes.  There can be at most one of each, and those are the only
  # input types supported.  make_names.pl is required and at least one of tags
  # and attributes is required.
  make_names_input = None
  tag_input = None
  attr_input = None
  for input in inputs:
    # Make input pathnames absolute so they can be accessed after changing
    # directory.  On Windows, convert \ to / for inputs to the perl script to
    # work around the intermix of activepython + cygwin perl.
    input_abs = os.path.abspath(input)
    input_abs_posix = input_abs.replace(os.path.sep, posixpath.sep)
    input_basename = os.path.basename(input)
    if input_basename == 'make_names.pl':
      assert make_names_input == None
      make_names_input = input_abs
    elif input_basename.endswith('TagNames.in') or \
         input_basename.endswith('tags.in'):
      assert tag_input == None
      tag_input = input_abs_posix
    elif input_basename.endswith('AttributeNames.in') or \
         input_basename.endswith('attrs.in'):
      assert attr_input == None
      attr_input = input_abs_posix
    else:
      assert False

  assert make_names_input != None
  assert tag_input != None or attr_input != None

  # scripts_path is a Perl include directory, located relative to
  # make_names_input.
  scripts_path = os.path.normpath(
      os.path.join(os.path.dirname(make_names_input), os.pardir,
                   'bindings', 'scripts'))

  # Change to the output directory because make_names.pl puts output in its
  # working directory.
  os.chdir(output_dir)

  # Build up the command.
  command = ['perl', '-I', scripts_path, make_names_input]
  if tag_input != None:
    command.extend(['--tags', tag_input])
  if attr_input != None:
    command.extend(['--attrs', attr_input])
  command.extend(options)

  # Do it.  check_call is new in 2.5, so simulate its behavior with call and
  # assert.
  return_code = subprocess.call(command)
  assert return_code == 0

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
