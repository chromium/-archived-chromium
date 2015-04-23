# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script takes an input JSON, and filters out all system call events that
# took less than 0.2ms.  This helps trim down the JSON data to only the most
# interesting / time critical events.

import sys
import re

def parseEvents(z):
  print 'parseEvents(['
  for e in z:
    if e.has_key('ms') and e.has_key('done'):
      dur = e['done'] - e['ms']
      if dur < 0.2:
        continue
    # Ugly regex to remove the L suffix on large python numbers.
    print '%s,' % re.sub('([0-9])L\\b', '\\1', str(e))
  print '])'

execfile(sys.argv[1])
