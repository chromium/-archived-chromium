#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

directory = os.path.dirname(__file__)
while not os.path.exists(os.path.join(directory, 'scripts')):
  directory = os.path.join(directory, '..')
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
sys.path.append(os.path.join(directory, 'scripts', 'common'))
sys.path.append(os.path.join(directory, 'pylibs'))

import try_job
from buildbot.scheduler import BadJobfile

class FileMock:
  name = 'user-test.diff'
  def read(self):
    return 'Dummy file content'


class SlaveMock:
  def __init__(self, is_available=True, empty=False):
    self.is_available = is_available
    if not empty:
      self.slave = True
    else:
      self.slave = None

  def isAvailable(self):
    return self.is_available

class SlavesMock:
  def __init__(self, empty=False, avail=True):
    if not empty:
      self.slaves = [SlaveMock(avail)]
    else:
      self.slaves = []


class BotMasterMock:
  def __init__(self, avail, busy, dead):
    if dead:
      self.builders = {
        'a' : SlavesMock(empty=True),
        'd' : SlavesMock(empty=True),
        'f' : SlavesMock(empty=True),
      }
    else:
      self.builders = {
          'a' : SlavesMock(avail=True),
          'b' : SlavesMock(avail=avail and not busy),
          'c' : SlavesMock(avail=avail and not busy),
          'd' : SlavesMock(avail=False),
          'e' : SlavesMock(avail=True and not busy),
          'f' : SlavesMock(empty=True),
          'g' : SlavesMock(avail=False),
      }


class ParentMock:
  def __init__(self, avail, busy, dead):
    self.botmaster = BotMasterMock(avail, busy, dead)


class TryJobTest(unittest.TestCase):
  def setUp(self):
    """Setup the pools.
    'f' should never be selected since it's empty."""
    self.pools = [['a', 'b', 'c'], ['d', 'e'], ['f']]

  def testParseJobGroup1(self):
    tryjob = try_job.TryJob(name='test1',
                    jobdir='test2',
                    pools=self.pools)
    tryjob.parent = ParentMock(False, False, False)
    (builderNames, ss, buildsetID) = tryjob.parseJob(FileMock())
    # avail: a, e.
    # not avail: b, c, d, g.
    # empty = f.
    self.assertEquals(builderNames, ['a', 'e'])
    self.assertEquals(buildsetID, 'user-test.diff')

  def testParseJobGroup2(self):
    tryjob = try_job.TryJob(name='test1',
                    jobdir='test2',
                    pools=self.pools)
    tryjob.parent = ParentMock(True, False, False)
    (builderNames, ss, buildsetID) = tryjob.parseJob(FileMock())
    # avail: a, b, c, e.
    # not avail:  d, g.
    # empty = f.
    self.assertEquals(builderNames, ['a', 'e'])
    self.assertEquals(buildsetID, 'user-test.diff')

  def testParseJobGroupBusy(self):
    tryjob = try_job.TryJob(name='test1',
                    jobdir='test2',
                    pools=self.pools)
    tryjob.parent = ParentMock(False, True, False)
    (builderNames, ss, buildsetID) = tryjob.parseJob(FileMock())
    # avail: a.
    # not avail:  b, c, d, e, g.
    # empty = f.
    self.assertEquals(builderNames[0], 'a')
    self.assertTrue(builderNames[1] in ('d', 'e'))

  def testParseJobGroupDead(self):
    tryjob = try_job.TryJob(name='test1',
                    jobdir='test2',
                    pools=self.pools)
    tryjob.parent = ParentMock(False, False, True)
    # no bots are available. None can be selected.
    self.assertRaises(BadJobfile, tryjob.parseJob, FileMock())


if __name__ == '__main__':
  unittest.main()
  