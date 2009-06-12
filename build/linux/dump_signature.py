#!/usr/bin/python2.4
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This generates symbol signatures with the same algorithm as
# src/breakpad/linux/minidump_writer.cc@17081

import sys

if len(sys.argv) != 2:
  sys.stderr.write("Error, no filename specified.\n")
  sys.exit(1)

bin = open(sys.argv[1])
data = bin.read(4096)
if len(data) != 4096:
  sys.stderr.write("Error, did not read first page of data.\n");
bin.close()

signature = [0] * 16
for i in range(0, 4096):
  signature[i % 16] ^= ord(data[i])

out = ''
# Assume we're running on little endian
for i in [3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15]:
  out += '%02X' % signature[i]
out += '0'
sys.stdout.write(out)
