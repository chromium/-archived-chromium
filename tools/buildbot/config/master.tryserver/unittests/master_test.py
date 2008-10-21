#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

MASTER_SCRIPT = os.path.abspath('../master.cfg')
directory = os.path.dirname(__file__)
while not os.path.exists(os.path.join(directory, 'scripts')):
  directory = os.path.join(directory, '..')

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
sys.path.append(os.path.join(directory, 'scripts', 'common'))
sys.path.append(os.path.join(directory, 'scripts', 'master'))
sys.path.append(os.path.join(directory, 'pylibs'))


def ReadFile(filename):
  """Returns the contents of a file."""
  file = open(filename, 'r')
  result = file.read()
  file.close()
  return result


def WriteFile(filename, contents):
  """Overwrites the file with the given contents."""
  file = open(filename, 'w')
  file.write(contents)
  file.close()


def ExecuteMasterScript():
  """Execute the master script and returns its dictionary."""
  script_locals = {}
  exec(ReadFile(MASTER_SCRIPT), script_locals)
  return script_locals


class MasterScriptTest(unittest.TestCase):
  def setUp(self):
    self.bot_password = os.path.join(directory, 'scripts', 'common',
                                     '.bot_password')
    self.existed = os.path.exists(self.bot_password)
    if not self.existed:
      WriteFile(self.bot_password, 'bleh')

  def tearDown(self):
    if not self.existed:
      os.remove(self.bot_password)
  
  def testParse(self):
    script_locals = ExecuteMasterScript()
    self.assertNotEqual(script_locals['BuildmasterConfig']['slavePortnum'], 0)
    self.assertTrue(len(script_locals['BuildmasterConfig']['slaves']) > 0)
    self.assertTrue(len(script_locals['BuildmasterConfig']['builders']) > 0)
    self.assertTrue(len(script_locals['BuildmasterConfig']['change_source'])
                    > 0)
    self.assertTrue(len(script_locals['BuildmasterConfig']['schedulers']) > 0)
    self.assertTrue(len(script_locals['BuildmasterConfig']['status']) > 0)
    self.assertTrue(len(script_locals['BuildmasterConfig']['buildbotURL']) > 0)


if __name__ == '__main__':
  unittest.main()
  