#!/usr/bin/python

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: rule_bison.py INPUT_FILE OUTPUT_DIR
# INPUT_FILE is a path to either CSSGrammar.y or XPathGrammar.y.
# OUTPUT_DIR is where the bison-generated .cpp and .h files should be placed.

import errno
import os
import os.path
import subprocess
import sys

assert len(sys.argv) == 3

input_file = sys.argv[1]
output_dir = sys.argv[2]

input_name = os.path.basename(input_file)
assert input_name == 'CSSGrammar.y' or input_name == 'XPathGrammar.y'
prefix = {'CSSGrammar.y': 'cssyy', 'XPathGrammar.y': 'xpathyy'}[input_name]

(input_root, input_ext) = os.path.splitext(input_name)

# The generated .h will be in a different location depending on the bison
# version.
output_h_tries = [
  os.path.join(output_dir, input_root + '.cpp.h'),
  os.path.join(output_dir, input_root + '.hpp'),
]

for output_h_try in output_h_tries:
  try:
    os.unlink(output_h_try)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise

output_cpp = os.path.join(output_dir, input_root + '.cpp')

return_code = subprocess.call(['bison', '-d', '-p', prefix, input_file,
                               '-o', output_cpp])
assert return_code == 0

# Find the name that bison used for the generated header file.
output_h_tmp = None
for output_h_try in output_h_tries:
  try:
    os.stat(output_h_try)
    output_h_tmp = output_h_try
    break
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise

assert output_h_tmp != None

# Read the header file in under the generated name and remove it.
output_h_file = open(output_h_tmp)
output_h_contents = output_h_file.read()
output_h_file.close()
os.unlink(output_h_tmp)

# Rewrite the generated header with #include guards.
output_h = os.path.join(output_dir, input_root + '.h')

output_h_file = open(output_h, 'w')
print >>output_h_file, '#ifndef %s_h' % input_root
print >>output_h_file, '#define %s_h' % input_root
print >>output_h_file, output_h_contents
print >>output_h_file, '#endif'
output_h_file.close()
