#!/usr/bin/python
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

'''Reads the Webkit Version.xcconfig file looking for MAJOR_VERSION and
MINOR_VERSION, emitting them into a webkit_version.h header file as
WEBKIT_MAJOR_VERSION and WEBKIT_MINOR_VERSION macros.
'''

import os
import re
import sys

def ReadVersionFile(fname):
  '''Reads the Webkit Version.xcconfig file looking for MAJOR_VERSION and
     MINOR_VERSION.  This function doesn't attempt to support the full syntax
     of xcconfig files.'''
  re_major = re.compile('MAJOR_VERSION\s*=\s*(\d+).*')
  re_minor = re.compile('MINOR_VERSION\s*=\s*(\d+).*')
  major = -1
  minor = -1
  f = open(fname, 'rb')
  line = "not empty"
  while line and not (major >= 0 and minor >= 0):
    line = f.readline()
    if major == -1:
      match = re_major.match(line)
      if match:
        major = int(match.group(1))
        continue
    if minor == -1:
      match = re_minor.match(line)
      if match:
        minor = int(match.group(1))
        continue
  assert(major >= 0 and minor >= 0)
  return (major, minor)

def EmitVersionHeader(version_file, output_dir):
  '''Given webkit's version file, emit a header file that we can use from
  within webkit_glue.cc.
  '''
  (major, minor) = ReadVersionFile(version_file)
  fname = os.path.join(output_dir, "webkit_version.h")
  f = open(fname, 'wb')
  template = """// webkit_version.h
// generated from %s

#define WEBKIT_VERSION_MAJOR %d
#define WEBKIT_VERSION_MINOR %d
""" % (version_file, major, minor)
  f.write(template)
  f.close()

def main():
  EmitVersionHeader(sys.argv[1], sys.argv[2])


if __name__ == "__main__":
  main()
 
  