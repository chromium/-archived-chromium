#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.gather.muppet_strings'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import unittest

from grit.gather import muppet_strings

class MuppetStringsUnittest(unittest.TestCase):
  def testParsing(self):
    original = '''<strings><BLA desc="Says hello">hello!</BLA><BINGO>YEEEESSS!!!</BINGO></strings>'''
    gatherer = muppet_strings.MuppetStrings(original)
    gatherer.Parse()
    self.failUnless(len(gatherer.GetCliques()) == 2)
    self.failUnless(gatherer.Translate('en').replace('\n', '') == original)

  def testEscapingAndLinebreaks(self):
    original = ('''\
<strings>
<LINEBREAK desc="Howdie">Hello
there
how
are
you?</LINEBREAK>   <ESCAPED meaning="bingo">4 &lt; 6</ESCAPED>
</strings>''')
    gatherer = muppet_strings.MuppetStrings(original)
    gatherer.Parse()
    self.failUnless(gatherer.GetCliques()[0].translateable)
    self.failUnless(len(gatherer.GetCliques()) == 2)
    self.failUnless(gatherer.GetCliques()[0].GetMessage().GetRealContent() ==
                    'Hello\nthere\nhow\nare\nyou?')
    self.failUnless(gatherer.GetCliques()[0].GetMessage().GetDescription() == 'Howdie')
    self.failUnless(gatherer.GetCliques()[1].GetMessage().GetRealContent() ==
                    '4 < 6')
    self.failUnless(gatherer.GetCliques()[1].GetMessage().GetMeaning() == 'bingo')

  def testPlaceholders(self):
    original = "<strings><MESSAGE translateable='True'>Hello [![USER]!] how are you? [![HOUR]!]:[![MINUTE]!]</MESSAGE></strings>"
    gatherer = muppet_strings.MuppetStrings(original)
    gatherer.Parse()
    self.failUnless(gatherer.GetCliques()[0].translateable)
    msg = gatherer.GetCliques()[0].GetMessage()
    self.failUnless(len(msg.GetPlaceholders()) == 3)
    ph = msg.GetPlaceholders()[0]
    self.failUnless(ph.GetOriginal() == '[![USER]!]')
    self.failUnless(ph.GetPresentation() == 'USER')

  def testTranslateable(self):
    original = "<strings><BINGO translateable='false'>Yo yo hi there</BINGO></strings>"
    gatherer = muppet_strings.MuppetStrings(original)
    gatherer.Parse()
    msg = gatherer.GetCliques()[0].GetMessage()
    self.failUnless(msg.GetRealContent() == "Yo yo hi there")
    self.failUnless(not gatherer.GetCliques()[0].translateable)

if __name__ == '__main__':
  unittest.main()

