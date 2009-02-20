# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

execfile(os.path.join(
  os.path.dirname(os.path.join(os.path.curdir, __file__)),
  'syscalls.py'))

def parseEvents(z):
  calls = { }
  for e in z:
    if e['eventtype'] == 'EVENT_TYPE_SYSCALL' and e['syscall'] == 17:
      delta = e['done'] - e['ms']
      tid = e['thread']
      ms = e['ms']
      print '%f - %f - %x' % (
          delta, ms, tid)

execfile(sys.argv[1])
