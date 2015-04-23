#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for the admin template gatherer.'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import StringIO
import tempfile
import unittest

from grit.gather import admin_template
from grit import util
from grit import grd_reader
from grit import grit_runner
from grit.tool import build


class AdmGathererUnittest(unittest.TestCase):
  def testParsingAndTranslating(self):
    pseudofile = StringIO.StringIO(
      'bingo bongo\n'
      'ding dong\n'
      '[strings] \n'
      'whatcha="bingo bongo"\n'
      'gotcha = "bingolabongola "the wise" fingulafongula" \n')
    gatherer = admin_template.AdmGatherer.FromFile(pseudofile)
    gatherer.Parse()
    self.failUnless(len(gatherer.GetCliques()) == 2)
    self.failUnless(gatherer.GetCliques()[1].GetMessage().GetRealContent() ==
                    'bingolabongola "the wise" fingulafongula')

    translation = gatherer.Translate('en')
    self.failUnless(translation == gatherer.GetText().strip())

  def testErrorHandling(self):
    pseudofile = StringIO.StringIO(
      'bingo bongo\n'
      'ding dong\n'
      'whatcha="bingo bongo"\n'
      'gotcha = "bingolabongola "the wise" fingulafongula" \n')
    gatherer = admin_template.AdmGatherer.FromFile(pseudofile)
    self.assertRaises(admin_template.MalformedAdminTemplateException,
                      gatherer.Parse)

  _TRANSLATABLES_FROM_FILE = (
    'Google', 'Google Desktop Search', 'Preferences',
    'Controls Google Deskop Search preferences',
    'Indexing and Capture Control',
    'Controls what files, web pages, and other content will be indexed by Google Desktop Search.',
    'Prevent indexing of e-mail',
    # there are lots more but we don't check any further
  )

  def VerifyCliquesFromAdmFile(self, cliques):
    self.failUnless(len(cliques) > 20)
    for ix in range(len(self._TRANSLATABLES_FROM_FILE)):
      text = cliques[ix].GetMessage().GetRealContent()
      self.failUnless(text == self._TRANSLATABLES_FROM_FILE[ix])

  def testFromFile(self):
    fname = util.PathFromRoot('grit/test/data/GoogleDesktopSearch.adm')
    gatherer = admin_template.AdmGatherer.FromFile(fname)
    gatherer.Parse()
    cliques = gatherer.GetCliques()
    self.VerifyCliquesFromAdmFile(cliques)

  def MakeGrd(self):
    grd = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3">
        <release seq="3">
          <structures>
            <structure type="admin_template" name="IDAT_GOOGLE_DESKTOP_SEARCH"
              file="GoogleDesktopSearch.adm" exclude_from_rc="true" />
            <structure type="txt" name="BINGOBONGO"
              file="README.txt" exclude_from_rc="true" />
          </structures>
        </release>
        <outputs>
          <output filename="de_res.rc" type="rc_all" lang="de" />
        </outputs>
      </grit>'''), util.PathFromRoot('grit/test/data'))
    grd.RunGatherers(recursive=True)
    return grd

  def testInGrd(self):
    grd = self.MakeGrd()
    cliques = grd.children[0].children[0].children[0].GetCliques()
    self.VerifyCliquesFromAdmFile(cliques)

  def testFileIsOutput(self):
    grd = self.MakeGrd()
    dirname = tempfile.mkdtemp()
    try:
      tool = build.RcBuilder()
      tool.o = grit_runner.Options()
      tool.output_directory = dirname
      tool.res = grd
      tool.Process()

      self.failUnless(os.path.isfile(
        os.path.join(dirname, 'de_GoogleDesktopSearch.adm')))
      self.failUnless(os.path.isfile(
        os.path.join(dirname, 'de_README.txt')))
    finally:
      for f in os.listdir(dirname):
        os.unlink(os.path.join(dirname, f))
      os.rmdir(dirname)

if __name__ == '__main__':
  unittest.main()

