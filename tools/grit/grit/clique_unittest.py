#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.clique'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import re
import StringIO
import unittest

from grit import clique
from grit import exception
from grit import pseudo
from grit import tclib
from grit import grd_reader
from grit import util

class MessageCliqueUnittest(unittest.TestCase):
  def testClique(self):
    factory = clique.UberClique()
    msg = tclib.Message(text='Hello USERNAME, how are you?',
                        placeholders=[
                          tclib.Placeholder('USERNAME', '%s', 'Joi')])
    c = factory.MakeClique(msg)

    self.failUnless(c.GetMessage() == msg)
    self.failUnless(c.GetId() == msg.GetId())

    msg_fr = tclib.Translation(text='Bonjour USERNAME, comment ca va?',
                               id=msg.GetId(), placeholders=[
                                tclib.Placeholder('USERNAME', '%s', 'Joi')])
    msg_de = tclib.Translation(text='Guten tag USERNAME, wie geht es dir?',
                               id=msg.GetId(), placeholders=[
                                tclib.Placeholder('USERNAME', '%s', 'Joi')])

    c.AddTranslation(msg_fr, 'fr')
    factory.FindCliqueAndAddTranslation(msg_de, 'de')

    # sort() sorts lists in-place and does not return them
    for lang in ('en', 'fr', 'de'):
      self.failUnless(lang in c.clique)

    self.failUnless(c.MessageForLanguage('fr').GetRealContent() ==
                    msg_fr.GetRealContent())

    try:
      c.MessageForLanguage('zh-CN', False)
      self.fail('Should have gotten exception')
    except:
      pass

    self.failUnless(c.MessageForLanguage('zh-CN', True) != None)

    rex = re.compile('fr|de|bingo')
    self.failUnless(len(c.AllMessagesThatMatch(rex, False)) == 2)
    self.failUnless(c.AllMessagesThatMatch(rex, True)[pseudo.PSEUDO_LANG] != None)

  def testBestClique(self):
    factory = clique.UberClique()
    factory.MakeClique(tclib.Message(text='Alfur', description='alfaholl'))
    factory.MakeClique(tclib.Message(text='Alfur', description=''))
    factory.MakeClique(tclib.Message(text='Vaettur', description=''))
    factory.MakeClique(tclib.Message(text='Vaettur', description=''))
    factory.MakeClique(tclib.Message(text='Troll', description=''))
    factory.MakeClique(tclib.Message(text='Gryla', description='ID: IDS_GRYLA'))
    factory.MakeClique(tclib.Message(text='Gryla', description='vondakerling'))
    factory.MakeClique(tclib.Message(text='Leppaludi', description='ID: IDS_LL'))
    factory.MakeClique(tclib.Message(text='Leppaludi', description=''))

    count_best_cliques = 0
    for c in factory.BestCliquePerId():
      count_best_cliques += 1
      msg = c.GetMessage()
      text = msg.GetRealContent()
      description = msg.GetDescription()
      if text == 'Alfur':
        self.failUnless(description == 'alfaholl')
      elif text == 'Gryla':
        self.failUnless(description == 'vondakerling')
      elif text == 'Leppaludi':
        self.failUnless(description == 'ID: IDS_LL')
    self.failUnless(count_best_cliques == 5)

  def testAllInUberClique(self):
    resources = grd_reader.Parse(util.WrapInputStream(
      StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
  <release seq="3">
    <messages>
      <message name="IDS_GREETING" desc="Printed to greet the currently logged in user">
        Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
      </message>
    </messages>
    <structures>
      <structure type="dialog" name="IDD_ABOUTBOX" encoding="utf-16" file="grit/test/data/klonk.rc" />
      <structure type="tr_html" name="ID_HTML" file="grit/test/data/simple.html" />
    </structures>
  </release>
</grit>''')), util.PathFromRoot('.'))
    resources.RunGatherers(True)
    content_list = []
    for clique_list in resources.UberClique().cliques_.values():
      for clique in clique_list:
        content_list.append(clique.GetMessage().GetRealContent())
    self.failUnless('Hello %s, how are you doing today?' in content_list)
    self.failUnless('Jack "Black" Daniels' in content_list)
    self.failUnless('Hello!' in content_list)

  def testCorrectExceptionIfWrongEncodingOnResourceFile(self):
    '''This doesn't really belong in this unittest file, but what the heck.'''
    resources = grd_reader.Parse(util.WrapInputStream(
      StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
  <release seq="3">
    <structures>
      <structure type="dialog" name="IDD_ABOUTBOX" file="grit/test/data/klonk.rc" />
    </structures>
  </release>
</grit>''')), util.PathFromRoot('.'))
    self.assertRaises(exception.SectionNotFound, resources.RunGatherers, True)

  def testSemiIdenticalCliques(self):
    messages = [
      tclib.Message(text='Hello USERNAME',
                    placeholders=[tclib.Placeholder('USERNAME', '$1', 'Joi')]),
      tclib.Message(text='Hello USERNAME',
                    placeholders=[tclib.Placeholder('USERNAME', '%s', 'Joi')]),
    ]
    self.failUnless(messages[0].GetId() == messages[1].GetId())

    # Both of the above would share a translation.
    translation = tclib.Translation(id=messages[0].GetId(),
                                    text='Bonjour USERNAME',
                                    placeholders=[tclib.Placeholder(
                                      'USERNAME', '$1', 'Joi')])

    factory = clique.UberClique()
    cliques = [factory.MakeClique(msg) for msg in messages]

    for clq in cliques:
      clq.AddTranslation(translation, 'fr')

    self.failUnless(cliques[0].MessageForLanguage('fr').GetRealContent() ==
                    'Bonjour $1')
    self.failUnless(cliques[1].MessageForLanguage('fr').GetRealContent() ==
                    'Bonjour %s')

  def testMissingTranslations(self):
    messages = [ tclib.Message(text='Hello'), tclib.Message(text='Goodbye') ]
    factory = clique.UberClique()
    cliques = [factory.MakeClique(msg) for msg in messages]

    cliques[1].MessageForLanguage('fr', False, True)

    self.failUnless(not factory.HasMissingTranslations())

    cliques[0].MessageForLanguage('de', False, False)

    self.failUnless(factory.HasMissingTranslations())

    report = factory.MissingTranslationsReport()
    self.failUnless(report.count('WARNING') == 1)
    self.failUnless(report.count('8053599568341804890 "Goodbye" fr') == 1)
    self.failUnless(report.count('ERROR') == 1)
    self.failUnless(report.count('800120468867715734 "Hello" de') == 1)

  def testCustomTypes(self):
    factory = clique.UberClique()
    message = tclib.Message(text='Bingo bongo')
    c = factory.MakeClique(message)
    try:
      c.SetCustomType(DummyCustomType())
      self.fail()
    except:
      pass  # expected case - 'Bingo bongo' does not start with 'jjj'

    message = tclib.Message(text='jjjBingo bongo')
    c = factory.MakeClique(message)
    c.SetCustomType(util.NewClassInstance(
      'grit.clique_unittest.DummyCustomType', clique.CustomType))
    translation = tclib.Translation(id=message.GetId(), text='Bilingo bolongo')
    c.AddTranslation(translation, 'fr')
    self.failUnless(c.MessageForLanguage('fr').GetRealContent().startswith('jjj'))


class DummyCustomType(clique.CustomType):
  def Validate(self, message):
    return message.GetRealContent().startswith('jjj')
  def ValidateAndModify(self, lang, translation):
    is_ok = self.Validate(translation)
    self.ModifyEachTextPart(lang, translation)
  def ModifyTextPart(self, lang, text):
    return 'jjj%s' % text


if __name__ == '__main__':
  unittest.main()
