#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.node.custom.filename'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../..'))

import unittest
from grit.node.custom import filename
from grit import clique
from grit import tclib


class WindowsFilenameUnittest(unittest.TestCase):

  def testValidate(self):
    factory = clique.UberClique()
    msg = tclib.Message(text='Bingo bongo')
    c = factory.MakeClique(msg)
    c.SetCustomType(filename.WindowsFilename())
    translation = tclib.Translation(id=msg.GetId(), text='Bilingo bolongo:')
    c.AddTranslation(translation, 'fr')
    self.failUnless(c.MessageForLanguage('fr').GetRealContent() == 'Bilingo bolongo ')


if __name__ == '__main__':
  unittest.main()

