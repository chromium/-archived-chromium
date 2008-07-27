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
