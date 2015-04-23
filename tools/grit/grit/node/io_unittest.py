#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for io.FileNode'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import os
import StringIO
import unittest

from grit.node import misc
from grit.node import io
from grit.node import empty
from grit import grd_reader
from grit import util


class FileNodeUnittest(unittest.TestCase):
  def testGetPath(self):
    root = misc.GritNode()
    root.StartParsing(u'grit', None)
    root.HandleAttribute(u'latest_public_release', u'0')
    root.HandleAttribute(u'current_release', u'1')
    root.HandleAttribute(u'base_dir', ur'..\resource')
    translations = empty.TranslationsNode()
    translations.StartParsing(u'translations', root)
    root.AddChild(translations)
    file_node = io.FileNode()
    file_node.StartParsing(u'file', translations)
    file_node.HandleAttribute(u'path', ur'flugel\kugel.pdf')
    translations.AddChild(file_node)
    root.EndParsing()

    self.failUnless(file_node.GetFilePath() ==
                    util.normpath(
                      os.path.join(ur'../resource', ur'flugel/kugel.pdf')))

  def testLoadTranslations(self):
    grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <translations>
          <file path="fr.xtb" lang="fr" />
        </translations>
        <release seq="3">
          <messages>
            <message name="ID_HELLO">Hello!</message>
            <message name="ID_HELLO_USER">Hello <ph name="USERNAME">%s<ex>Joi</ex></ph></message>
          </messages>
        </release>
      </grit>'''), util.PathFromRoot('grit/test/data'))
    grd.RunGatherers(recursive=True)
    self.failUnless(True)


if __name__ == '__main__':
  unittest.main()
