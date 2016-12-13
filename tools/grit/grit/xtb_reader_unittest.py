#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

