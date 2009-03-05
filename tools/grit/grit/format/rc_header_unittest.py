#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for the rc_header formatter'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import StringIO
import unittest

from grit.format import rc_header
from grit.node import message
from grit.node import structure
from grit.node import include
from grit.node import misc
from grit import grd_reader
from grit import exception


class RcHeaderFormatterUnittest(unittest.TestCase):
  def setUp(self):
    self.formatter = rc_header.Item()
    self.formatter.ids_ = {}  # need to reset this between tests

  def FormatAll(self, grd):
    output = []
    for node in grd:
      if isinstance(node, (message.MessageNode, structure.StructureNode,
                           include.IncludeNode, misc.IdentifierNode)):
        output.append(self.formatter.Format(node))
    output = ''.join(output)
    return output.replace(' ', '')

  def testFormatter(self):
    grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en" current_release="3" base_dir=".">
        <release seq="3">
          <includes first_id="300" comment="bingo">
            <include type="gif" name="ID_LOGO" file="images/logo.gif" />
          </includes>
          <messages first_id="10000">
            <message name="IDS_GREETING" desc="Printed to greet the currently logged in user">
              Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
            </message>
            <message name="IDS_BONGO">
              Bongo!
            </message>
          </messages>
          <structures>
            <structure type="dialog" name="IDD_NARROW_DIALOG" file="rc_files/dialogs.rc" />
            <structure type="version" name="VS_VERSION_INFO" file="rc_files/version.rc" />
          </structures>
        </release>
      </grit>'''), '.')
    output = self.FormatAll(grd)
    self.failUnless(output.count('IDS_GREETING10000'))
    self.failUnless(output.count('ID_LOGO300'))

  def testExplicitFirstIdOverlaps(self):
    # second first_id will overlap preexisting range
    grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en" current_release="3" base_dir=".">
        <release seq="3">
          <includes first_id="300" comment="bingo">
            <include type="gif" name="ID_LOGO" file="images/logo.gif" />
            <include type="gif" name="ID_LOGO2" file="images/logo2.gif" />
          </includes>
          <messages first_id="301">
            <message name="IDS_GREETING" desc="Printed to greet the currently logged in user">
              Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
            </message>
            <message name="IDS_SMURFGEBURF">Frubegfrums</message>
          </messages>
        </release>
      </grit>'''), '.')
    self.assertRaises(exception.IdRangeOverlap, self.FormatAll, grd)

  def testImplicitOverlapsPreexisting(self):
    # second message in <messages> will overlap preexisting range
    grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en" current_release="3" base_dir=".">
        <release seq="3">
          <includes first_id="301" comment="bingo">
            <include type="gif" name="ID_LOGO" file="images/logo.gif" />
            <include type="gif" name="ID_LOGO2" file="images/logo2.gif" />
          </includes>
          <messages first_id="300">
            <message name="IDS_GREETING" desc="Printed to greet the currently logged in user">
              Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
            </message>
            <message name="IDS_SMURFGEBURF">Frubegfrums</message>
          </messages>
        </release>
      </grit>'''), '.')
    self.assertRaises(exception.IdRangeOverlap, self.FormatAll, grd)


if __name__ == '__main__':
  unittest.main()
