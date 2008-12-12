#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

if len(sys.argv) != 2:
  print """Usage: sort_sln.py <SOLUTIONNAME>.sln
to sort the solution file to a normalized scheme. Do this before checking in
changes to a solution file to avoid having a lot of unnecessary diffs."""
  sys.exit(1)

filename = sys.argv[1]
print "Sorting " + filename;

try:
  sln = open(filename, "r");
except IOError:
  print "Unable to open " + filename + " for reading."
  sys.exit(1)

output = ""
seclines = None
while 1:
  line = sln.readline()
  if not line:
    break

  if seclines is not None:
    # Process the end of a section, dump the sorted lines
    if line.lstrip().startswith('End'):
      output = output + ''.join(sorted(seclines))
      seclines = None
    # Process within a section
    else:
      seclines.append(line)
      continue

  # Process the start of a section
  if (line.lstrip().startswith('GlobalSection') or
      line.lstrip().startswith('ProjectSection')):
    if seclines: raise Exception('Already in a section')
    seclines = []

  output = output + line

sln.close()
try:
  sln = open(filename, "w")
  sln.write(output)
except IOError:
  print "Unable to write to " + filename
  sys.exit(1);
print "Done."

