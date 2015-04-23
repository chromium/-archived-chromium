#!/usr/bin/python

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: rule_gperf.py INPUT_FILE OUTPUT_DIR
# INPUT_FILE is a path to DocTypeStrings.gperf, HTMLEntityNames.gperf, or
# ColorData.gperf.
# OUTPUT_DIR is where the gperf-generated .cpp file should be placed.  Because
# some users want a .c file instead of a .cpp file, the .cpp file is copied
# to .c when done.

import posixpath
import shutil
import subprocess
import sys

assert len(sys.argv) == 3

input_file = sys.argv[1]
output_dir = sys.argv[2]

gperf_commands = {
  'DocTypeStrings.gperf': [
    '-CEot', '-L', 'ANSI-C', '-k*', '-N', 'findDoctypeEntry',
    '-F', ',PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards'
  ],
  'HTMLEntityNames.gperf': [
    '-a', '-L', 'ANSI-C', '-C', '-G', '-c', '-o', '-t', '-k*',
    '-N', 'findEntity', '-D', '-s', '2'
  ],
  'ColorData.gperf': [
    '-CDEot', '-L', 'ANSI-C', '-k*', '-N', 'findColor', '-D', '-s', '2'
  ],
}

input_name = posixpath.basename(input_file)
assert input_name in gperf_commands

(input_root, input_ext) = posixpath.splitext(input_name)
output_cpp = posixpath.join(output_dir, input_root + '.cpp')

command = ['gperf', '--output-file', output_cpp]
command.extend(gperf_commands[input_name])
command.append(input_file)

# Do it.  check_call is new in 2.5, so simulate its behavior with call and
# assert.
return_code = subprocess.call(command)
assert return_code == 0

output_c = posixpath.join(output_dir, input_root + '.c')
shutil.copyfile(output_cpp, output_c)
