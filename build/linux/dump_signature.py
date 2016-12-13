#!/usr/bin/python2.4
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This generates symbol signatures with the same algorithm as
# src/breakpad/linux/minidump_writer.cc@17081

import sys
import struct

if len(sys.argv) != 2:
  sys.stderr.write("Error, no filename specified.\n")
  sys.exit(1)

bin = open(sys.argv[1])
data = bin.read(4096)
if len(data) != 4096:
  sys.stderr.write("Error, did not read first page of data.\n");
  sys.exit(1)
bin.close()

signature = [0] * 16
for i in range(0, 4096):
  signature[i % 16] ^= ord(data[i])

# Append a 0 at the end for the generation number (always 0 on Linux)
out = ('%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X0' %
      struct.unpack('I2H8B', struct.pack('16B', *signature)))
sys.stdout.write(out)
