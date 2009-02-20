# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

execfile(os.path.join(
  os.path.dirname(os.path.join(os.path.curdir, __file__)),
  'syscalls.py'))

def parseEvents(z):
  crits =  { }
  calls = { }
  for e in z:
    if (e['eventtype'] == 'EVENT_TYPE_ENTER_CS' or
        e['eventtype'] == 'EVENT_TYPE_TRYENTER_CS' or
        e['eventtype'] == 'EVENT_TYPE_LEAVE_CS'):
      cs = e['critical_section']
      if not crits.has_key(cs):
        crits[cs] = [ ]
      crits[cs].append(e)

#  for cs, es in crits.iteritems():
#    print 'cs: 0x%08x' % cs
#    for e in es:
#      print '  0x%08x - %s - %f' % (e['thread'], e['eventtype'], e['ms'])

  for cs, es in crits.iteritems():
    print 'cs: 0x%08x' % cs

    tid_stack = [ ]
    for e in es:
      if e['eventtype'] == 'EVENT_TYPE_ENTER_CS':
        tid_stack.append(e)
      elif e['eventtype'] == 'EVENT_TYPE_TRYENTER_CS':
        if e['retval'] != 0:
          tid_stack.append(e)
      elif e['eventtype'] == 'EVENT_TYPE_LEAVE_CS':
        if not tid_stack:
          raise repr(e)
        tid = tid_stack.pop()
        if tid['thread'] != e['thread']:
          raise repr(tid) + '--' + repr(e)

    # Critical section left locked?
    if tid_stack:
      #raise repr(tid_stack)
      pass

execfile(sys.argv[1])
