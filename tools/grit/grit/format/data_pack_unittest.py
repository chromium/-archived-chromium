#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.data_pack'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))
import unittest

from grit.format import data_pack

class FormatDataPackUnittest(unittest.TestCase):
  def testWriteDataPack(self):
    expected = ('\x01\x00\x00\x00\x04\x00\x00\x00\x01\x00\x00\x008\x00\x00'
        '\x00\x00\x00\x00\x00\x04\x00\x00\x008\x00\x00\x00\x0c\x00\x00\x00'
        '\x06\x00\x00\x00D\x00\x00\x00\x0c\x00\x00\x00\n\x00\x00\x00P\x00'
        '\x00\x00\x00\x00\x00\x00this is id 4this is id 6')
    input = { 1: "", 4: "this is id 4", 6: "this is id 6", 10: "" }
    output = data_pack.DataPack.WriteDataPack(input)
    self.failUnless(output == expected)


if __name__ == '__main__':
  unittest.main()

