# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Splits a single json file (read from stdin) into separate files of 40k
# records, named split.X.

import sys

filecount = 0;
count = 0;

f = open('split.0', 'wb');

for l in sys.stdin:
  if l == "},\r\n":
    count += 1
    if count == 40000:
      f.write("}]);\r\n")
      count = 0;
      filecount += 1
      f = open('split.%d' % filecount, 'wb');
      f.write("parseEvents([\r\n")
      continue
  f.write(l)
