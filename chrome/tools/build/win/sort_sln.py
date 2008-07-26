#!/usr/bin/env python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
