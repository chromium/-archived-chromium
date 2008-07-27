#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
