#!/usr/bin/python

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: action_maketokenizer.py OUTPUTS -- INPUTS
#
# Multiple INPUTS may be listed.  The sections are separated by -- arguments.
#
# OUTPUTS must contain a single item: a path to tokenizer.cpp.
#
# INPUTS must contain exactly two items.  The first item must be the path to
# maketokenizer.  The second item must be the path to tokenizer.flex.


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

  assert len(outputs) == 1
  output = outputs[0]

  assert len(inputs) == 2
  maketokenizer = inputs[0]
  flex_input = inputs[1]

  # Build up the command.
  command = 'flex -t %s | perl %s > %s' % (flex_input, maketokenizer, output)

  # Do it.  check_call is new in 2.5, so simulate its behavior with call and
  # assert.
  # TODO(mark): Don't use shell=True, build up the pipeline directly.
  p = subprocess.Popen(command, shell=True)
  return_code = p.wait()
  assert return_code == 0

  return return_code


if __name__ == '__main__':
  sys.exit(main(sys.argv))
