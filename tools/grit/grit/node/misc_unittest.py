#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for misc.GritNode'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import unittest
import StringIO

from grit import grd_reader
import grit.exception
from grit import util
from grit.node import misc


class GritNodeUnittest(unittest.TestCase):
  def testUniqueNameAttribute(self):
    try:
      restree = grd_reader.Parse(
        util.PathFromRoot('grit/test/data/duplicate-name-input.xml'))
      self.fail('Expected parsing exception because of duplicate names.')
    except grit.exception.Parsing:
      pass  # Expected case


class IfNodeUnittest(unittest.TestCase):
  def testIffyness(self):
    grd = grd_reader.Parse(StringIO.StringIO('''
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <release seq="3">
          <messages>
            <if expr="'bingo' in defs">
              <message name="IDS_BINGO">
                Bingo!
              </message>
            </if>
            <if expr="'hello' in defs">
              <message name="IDS_HELLO">
                Hello!
              </message>
            </if>
            <if expr="lang == 'fr' or 'FORCE_FRENCH' in defs">
              <message name="IDS_HELLO" internal_comment="French version">
                Good morning
              </message>
            </if>
          </messages>
        </release>
      </grit>'''), dir='.')

    messages_node = grd.children[0].children[0]
    bingo_message = messages_node.children[0].children[0]
    hello_message = messages_node.children[1].children[0]
    french_message = messages_node.children[2].children[0]
    assert bingo_message.name == 'message'
    assert hello_message.name == 'message'
    assert french_message.name == 'message'

    grd.SetOutputContext('fr', {'hello' : '1'})
    self.failUnless(not bingo_message.SatisfiesOutputCondition())
    self.failUnless(hello_message.SatisfiesOutputCondition())
    self.failUnless(french_message.SatisfiesOutputCondition())

    grd.SetOutputContext('en', {'bingo' : 1})
    self.failUnless(bingo_message.SatisfiesOutputCondition())
    self.failUnless(not hello_message.SatisfiesOutputCondition())
    self.failUnless(not french_message.SatisfiesOutputCondition())

    grd.SetOutputContext('en', {'FORCE_FRENCH' : '1', 'bingo' : '1'})
    self.failUnless(bingo_message.SatisfiesOutputCondition())
    self.failUnless(not hello_message.SatisfiesOutputCondition())
    self.failUnless(french_message.SatisfiesOutputCondition())


class ReleaseNodeUnittest(unittest.TestCase):
  def testPseudoControl(self):
    grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="1" source_lang_id="en-US" current_release="2" base_dir=".">
        <release seq="1" allow_pseudo="false">
          <messages>
            <message name="IDS_HELLO">
              Hello
            </message>
          </messages>
          <structures>
            <structure type="dialog" name="IDD_ABOUTBOX" encoding="utf-16" file="klonk.rc" />
          </structures>
        </release>
        <release seq="2">
          <messages>
            <message name="IDS_BINGO">
              Bingo
            </message>
          </messages>
          <structures>
            <structure type="menu" name="IDC_KLONKMENU" encoding="utf-16" file="klonk.rc" />
          </structures>
        </release>
      </grit>'''), util.PathFromRoot('grit/test/data'))
    grd.RunGatherers(recursive=True)

    hello = grd.GetNodeById('IDS_HELLO')
    aboutbox = grd.GetNodeById('IDD_ABOUTBOX')
    bingo = grd.GetNodeById('IDS_BINGO')
    menu = grd.GetNodeById('IDC_KLONKMENU')

    for node in [hello, aboutbox]:
      self.failUnless(not node.PseudoIsAllowed())

    for node in [bingo, menu]:
      self.failUnless(node.PseudoIsAllowed())

    for node in [hello, aboutbox]:
      try:
        formatter = node.ItemFormatter('rc_all')
        formatter.Format(node, 'xyz-pseudo')
        self.fail('Should have failed during Format since pseudo is not allowed')
      except:
        pass  # expected case

    for node in [bingo, menu]:
      try:
        formatter = node.ItemFormatter('rc_all')
        formatter.Format(node, 'xyz-pseudo')
      except:
        self.fail('Should not have gotten exception since pseudo is allowed')


if __name__ == '__main__':
  unittest.main()

