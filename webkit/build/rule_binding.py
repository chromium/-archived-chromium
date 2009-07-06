#!/usr/bin/python

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: rule_binding.py INPUT CPPDIR HDIR -- INPUTS -- OPTIONS
#
# INPUT is an IDL file, such as Whatever.idl.
#
# CPPDIR is the directory into which V8Whatever.cpp will be placed.  HDIR is
# the directory into which V8Whatever.h will be placed.
#
# The first item in INPUTS is the path to generate-bindings.pl.  Remaining
# items in INPUTS are used to build the Perl module include path.
#
# OPTIONS are passed as-is to generate-bindings.pl as additional arguments.


import errno
import os
import shlex
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
  assert len(sections) == 3
  (base, inputs, options) = sections

  assert len(base) == 3
  input = base[0]
  cppdir = base[1]
  hdir = base[2]

  assert len(inputs) > 1
  generate_bindings = inputs[0]
  perl_modules = inputs[1:]

  include_dirs = []
  for perl_module in perl_modules:
    include_dir = os.path.dirname(perl_module)
    if not include_dir in include_dirs:
      include_dirs.append(include_dir)

  # The defines come in as one flat string. Split it up into distinct arguments.
  if '--defines' in options:
    defines_index = options.index('--defines')
    if defines_index + 1 < len(options):
      split_options = shlex.split(options[defines_index + 1])
      if split_options:
        options[defines_index + 1] = ' '.join(split_options)

  # Build up the command.
  command = ['perl', '-w']
  for include_dir in include_dirs:
    command.extend(['-I', include_dir])
  command.append(generate_bindings)
  command.extend(options)
  command.extend(['--outputDir', cppdir, input])

  # Do it.  check_call is new in 2.5, so simulate its behavior with call and
  # assert.
  return_code = subprocess.call(command)
  assert return_code == 0

  # Both the .cpp and .h were generated in cppdir, but if hdir is different,
  # the .h needs to move.  Copy it instead of using os.rename for maximum
  # portability in all cases.
  if cppdir != hdir:
    input_basename = os.path.basename(input)
    (root, ext) = os.path.splitext(input_basename)
    hname = 'V8%s.h' % root
    hsrc = os.path.join(cppdir, hname)
    hdst = os.path.join(hdir, hname)
    shutil.copyfile(hsrc, hdst)
    os.unlink(hsrc)

  return return_code


if __name__ == '__main__':
  sys.exit(main(sys.argv))
