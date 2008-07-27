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

'''Unit tests for grit.xtb_reader'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import StringIO
import unittest

from grit import xtb_reader
from grit import clique
from grit import grd_reader
from grit import tclib
from grit import util


class XtbReaderUnittest(unittest.TestCase):
  def testParsing(self):
    xtb_file = StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE translationbundle>
      <translationbundle lang="fr">
        <translation id="5282608565720904145">Bingo.</translation>
        <translation id="2955977306445326147">Bongo longo.</translation>
        <translation id="238824332917605038">Hullo</translation>
        <translation id="6629135689895381486"><ph name="PROBLEM_REPORT"/> peut <ph name="START_LINK"/>utilisation excessive de majuscules<ph name="END_LINK"/>.</translation>
        <translation id="7729135689895381486">Hello
this is another line
and another

and another after a blank line.</translation>
      </translationbundle>''')
    
    messages = []
    def Callback(id, structure):
      messages.append((id, structure))
    xtb_reader.Parse(xtb_file, Callback)
    self.failUnless(len(messages[0][1]) == 1)
    self.failUnless(messages[3][1][0])  # PROBLEM_REPORT placeholder
    self.failUnless(messages[4][0] == '7729135689895381486')
    self.failUnless(messages[4][1][7][1] == 'and another after a blank line.')

  def testParsingIntoMessages(self):
    grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <messages>
        <message name="ID_MEGA">Fantastic!</message>
        <message name="ID_HELLO_USER">Hello <ph name="USERNAME">%s<ex>Joi</ex></ph></message>
      </messages>'''), dir='.', flexible_root=True)
    
    clique_mega = grd.children[0].GetCliques()[0]
    msg_mega = clique_mega.GetMessage()
    clique_hello_user = grd.children[1].GetCliques()[0]
    msg_hello_user = clique_hello_user.GetMessage()
    
    xtb_file = StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE translationbundle>
      <translationbundle lang="is">
        <translation id="%s">Meirihattar!</translation>
        <translation id="%s">Saelir <ph name="USERNAME"/></translation>
      </translationbundle>''' % (msg_mega.GetId(), msg_hello_user.GetId()))
    
    xtb_reader.Parse(xtb_file, grd.UberClique().GenerateXtbParserCallback('is'))
    self.failUnless(clique_mega.MessageForLanguage('is').GetRealContent() ==
                    'Meirihattar!')
    self.failUnless(clique_hello_user.MessageForLanguage('is').GetRealContent() ==
                    'Saelir %s')

  def testParseLargeFile(self):
    def Callback(id, structure):
      pass
    xtb = file(util.PathFromRoot('grit/test/data/fr.xtb'))
    xtb_reader.Parse(xtb, Callback)
    xtb.close()


if __name__ == '__main__':
  unittest.main()
