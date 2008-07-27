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

'''Unit tests for the 'grit transl2tc' tool.'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import StringIO
import unittest

from grit.tool import transl2tc
from grit import grd_reader
from grit import util


def MakeOptions():
  from grit import grit_runner
  return grit_runner.Options()


class TranslationToTcUnittest(unittest.TestCase):
  
  def testOutput(self):
    buf = StringIO.StringIO()
    tool = transl2tc.TranslationToTc()
    translations = [
      ['1', 'Hello USERNAME, how are you?'],
      ['12', 'Howdie doodie!'],
      ['123', 'Hello\n\nthere\n\nhow are you?'],
      ['1234', 'Hello is > goodbye but < howdie pardner'],
    ]
    tool.WriteTranslations(buf, translations)
    output = buf.getvalue()
    self.failUnless(output.strip() == '''
1 Hello USERNAME, how are you?
12 Howdie doodie!
123 Hello

there

how are you?
1234 Hello is &gt; goodbye but &lt; howdie pardner
'''.strip())

  def testExtractTranslations(self):
    path = util.PathFromRoot('grit/test/data')
    current_grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <release seq="3">
          <messages>
            <message name="IDS_SIMPLE">
              One
            </message>
            <message name="IDS_PLACEHOLDER">
              <ph name="NUMBIRDS">%s<ex>3</ex></ph> birds
            </message>
            <message name="IDS_PLACEHOLDERS">
              <ph name="ITEM">%d<ex>1</ex></ph> of <ph name="COUNT">%d<ex>3</ex></ph>
            </message>
            <message name="IDS_REORDERED_PLACEHOLDERS">
              <ph name="ITEM">$1<ex>1</ex></ph> of <ph name="COUNT">$2<ex>3</ex></ph>
            </message>
            <message name="IDS_CHANGED">
              This is the new version
            </message>
            <message name="IDS_TWIN_1">Hello</message>
            <message name="IDS_TWIN_2">Hello</message>
            <message name="IDS_NOT_TRANSLATEABLE" translateable="false">:</message>
            <message name="IDS_LONGER_TRANSLATED">
              Removed document <ph name="FILENAME">$1<ex>c:\temp</ex></ph>
            </message>
            <message name="IDS_DIFFERENT_TWIN_1">Howdie</message>
            <message name="IDS_DIFFERENT_TWIN_2">Howdie</message>
          </messages>
          <structures>
            <structure type="dialog" name="IDD_ABOUTBOX" encoding="utf-16" file="klonk.rc" />
            <structure type="menu" name="IDC_KLONKMENU" encoding="utf-16" file="klonk.rc" />
          </structures>
        </release>
      </grit>'''), path)
    current_grd.RunGatherers(recursive=True)
    
    source_rc_path = util.PathFromRoot('grit/test/data/source.rc')
    source_rc = file(source_rc_path).read()
    transl_rc_path = util.PathFromRoot('grit/test/data/transl.rc')
    transl_rc = file(transl_rc_path).read()
    
    tool = transl2tc.TranslationToTc()
    output_buf = StringIO.StringIO()
    globopts = MakeOptions()
    globopts.verbose = True
    globopts.output_stream = output_buf
    tool.Setup(globopts, [])
    translations = tool.ExtractTranslations(current_grd,
                                            source_rc, source_rc_path,
                                            transl_rc, transl_rc_path)
    
    values = translations.values()
    output = output_buf.getvalue()
    
    self.failUnless('Ein' in values)
    self.failUnless('NUMBIRDS Vogeln' in values)
    self.failUnless('ITEM von COUNT' in values)
    self.failUnless(values.count('Hallo') == 1)
    self.failIf('Dass war die alte Version' in values)
    self.failIf(':' in values)
    self.failIf('Dokument FILENAME ist entfernt worden' in values)
    self.failIf('Nicht verwendet' in values)
    self.failUnless(('Howdie' in values or 'Hallo sagt man' in values) and not
      ('Howdie' in values and 'Hallo sagt man' in values))
    
    self.failUnless('XX01XX&SkraXX02XX&HaettaXX03XXThetta er "Klonk" sem eg fylaXX04XXgonkurinnXX05XXKlonk && er "gott"XX06XX&HjalpXX07XX&Um...XX08XX' in values)
    
    self.failUnless('I lagi' in values)
    
    self.failUnless(output.count('Structure of message IDS_REORDERED_PLACEHOLDERS has changed'))
    self.failUnless(output.count('Message IDS_CHANGED has changed'))
    self.failUnless(output.count('Structure of message IDS_LONGER_TRANSLATED has changed'))
    self.failUnless(output.count('Two different translations for "Howdie"'))
    self.failUnless(output.count('IDD_DIFFERENT_LENGTH_IN_TRANSL has wrong # of cliques'))


if __name__ == '__main__':
  unittest.main()