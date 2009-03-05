#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for <structure> nodes.
'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import unittest
import StringIO

from grit.node import structure
from grit import grd_reader
from grit import util


class StructureUnittest(unittest.TestCase):
  def testSkeleton(self):
    grd = grd_reader.Parse(StringIO.StringIO(
      '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <release seq="3">
          <structures>
            <structure type="dialog" name="IDD_ABOUTBOX" file="klonk.rc" encoding="utf-16-le">
              <skeleton expr="lang == 'fr'" variant_of_revision="1" file="klonk-alternate-skeleton.rc" />
            </structure>
          </structures>
        </release>
      </grit>'''), dir=util.PathFromRoot('grit\\test\\data'))
    grd.RunGatherers(recursive=True)
    grd.output_language = 'fr'

    node = grd.GetNodeById('IDD_ABOUTBOX')
    formatter = node.ItemFormatter('rc_all')
    self.failUnless(formatter)
    transl = formatter.Format(node, 'fr')

    self.failUnless(transl.count('040704') and transl.count('110978'))
    self.failUnless(transl.count('2005",IDC_STATIC'))

  def testOutputEncoding(self):
    grd = grd_reader.Parse(StringIO.StringIO(
      '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <release seq="3">
          <structures>
            <structure type="dialog" name="IDD_ABOUTBOX" file="klonk.rc" encoding="utf-16-le" output_encoding="utf-8-sig" />
          </structures>
        </release>
      </grit>'''), dir=util.PathFromRoot('grit\\test\\data'))
    node = grd.GetNodeById('IDD_ABOUTBOX')
    self.failUnless(node._GetOutputEncoding() == 'utf-8')
    self.failUnless(node._ShouldAddBom())

if __name__ == '__main__':
  unittest.main()

