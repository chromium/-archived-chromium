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