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
    if e['eventtype'] == 'EVENT_TYPE_SYSCALL' and e['done'] > 0:
      delta = e['done'] - e['ms']
      syscall = e['syscall']
      tid = e['thread']
      ms = e['ms']
      calls[syscall] = calls.get(syscall, 0) + delta
      print '%f - %f - %x - %d %s' % (
          delta, ms, tid, syscall, syscalls.get(syscall, 'unknown'))

  #for syscall, delta in calls.items():
  #  print '%f - %d %s' % (delta, syscall, syscalls.get(syscall, 'unknown'))

execfile(sys.argv[1])
