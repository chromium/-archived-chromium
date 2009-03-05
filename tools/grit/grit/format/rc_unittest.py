#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.rc'''

import os
import re
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import tempfile
import unittest
import StringIO

from grit.format import rc
from grit import grd_reader
from grit import util
from grit.tool import build

class DummyOutput(object):
  def __init__(self, type, language):
    self.type = type
    self.language = language
  def GetType(self):
    return self.type
  def GetLanguage(self):
    return self.language
  def GetOutputFilename(self):
    return 'hello.gif'

class FormatRcUnittest(unittest.TestCase):
  def testMessages(self):
    root = grd_reader.Parse(StringIO.StringIO('''
      <messages>
          <message name="IDS_BTN_GO" desc="Button text" meaning="verb">Go!</message>
          <message name="IDS_GREETING" desc="Printed to greet the currently logged in user">
            Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
          </message>
          <message name="BONGO" desc="Flippo nippo">
            Howdie "Mr. Elephant", how are you doing?   \'\'\'
          </message>
          <message name="IDS_WITH_LINEBREAKS">
Good day sir,
I am a bee
Sting sting
          </message>
      </messages>
      '''), flexible_root = True)
    util.FixRootForUnittest(root)

    buf = StringIO.StringIO()
    build.RcBuilder.ProcessNode(root, DummyOutput('rc_all', 'en'), buf)
    output = buf.getvalue()
    self.failUnless(output.strip() == u'''
STRINGTABLE
BEGIN
  IDS_BTN_GO      "Go!"
  IDS_GREETING    "Hello %s, how are you doing today?"
  BONGO           "Howdie ""Mr. Elephant"", how are you doing?   "
  IDS_WITH_LINEBREAKS "Good day sir,\\nI am a bee\\nSting sting"
END'''.strip())


  def testRcSection(self):
    root = grd_reader.Parse(StringIO.StringIO('''
      <structures>
          <structure type="menu" name="IDC_KLONKMENU" file="grit\\test\data\klonk.rc" encoding="utf-16" />
          <structure type="dialog" name="IDD_ABOUTBOX" file="grit\\test\data\klonk.rc" encoding="utf-16" />
          <structure type="version" name="VS_VERSION_INFO" file="grit\\test\data\klonk.rc" encoding="utf-16" />
      </structures>'''), flexible_root = True)
    util.FixRootForUnittest(root)
    root.RunGatherers(recursive = True)

    buf = StringIO.StringIO()
    build.RcBuilder.ProcessNode(root, DummyOutput('rc_all', 'en'), buf)
    output = buf.getvalue()
    self.failUnless(output.strip() == u'''
IDC_KLONKMENU MENU
BEGIN
    POPUP "&File"
    BEGIN
        MENUITEM "E&xit",                       IDM_EXIT
        MENUITEM "This be ""Klonk"" me like",   ID_FILE_THISBE
        POPUP "gonk"
        BEGIN
            MENUITEM "Klonk && is ""good""",           ID_GONK_KLONKIS
        END
    END
    POPUP "&Help"
    BEGIN
        MENUITEM "&About ...",                  IDM_ABOUT
    END
END

IDD_ABOUTBOX DIALOGEX 22, 17, 230, 75
STYLE DS_SETFONT | DS_MODALFRAME | WS_CAPTION | WS_SYSMENU
CAPTION "About"
FONT 8, "System", 0, 0, 0x0
BEGIN
    ICON            IDI_KLONK,IDC_MYICON,14,9,20,20
    LTEXT           "klonk Version ""yibbee"" 1.0",IDC_STATIC,49,10,119,8,
                    SS_NOPREFIX
    LTEXT           "Copyright (C) 2005",IDC_STATIC,49,20,119,8
    DEFPUSHBUTTON   "OK",IDOK,195,6,30,11,WS_GROUP
    CONTROL         "Jack ""Black"" Daniels",IDC_RADIO1,"Button",
                    BS_AUTORADIOBUTTON,46,51,84,10
END

VS_VERSION_INFO VERSIONINFO
 FILEVERSION 1,0,0,1
 PRODUCTVERSION 1,0,0,1
 FILEFLAGSMASK 0x17L
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x4L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "FileDescription", "klonk Application"
            VALUE "FileVersion", "1, 0, 0, 1"
            VALUE "InternalName", "klonk"
            VALUE "LegalCopyright", "Copyright (C) 2005"
            VALUE "OriginalFilename", "klonk.exe"
            VALUE "ProductName", " klonk Application"
            VALUE "ProductVersion", "1, 0, 0, 1"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END'''.strip())


  def testRcIncludeStructure(self):
    root = grd_reader.Parse(StringIO.StringIO('''
      <structures>
        <structure type="tr_html" name="IDR_HTML" file="bingo.html"/>
        <structure type="tr_html" name="IDR_HTML2" file="bingo2.html"/>
      </structures>'''), flexible_root = True)
    util.FixRootForUnittest(root, '/temp')
    # We do not run gatherers as it is not needed and wouldn't find the file

    buf = StringIO.StringIO()
    build.RcBuilder.ProcessNode(root, DummyOutput('rc_all', 'en'), buf)
    output = buf.getvalue()
    expected = (u'IDR_HTML           HTML               "%s"\n'
                u'IDR_HTML2          HTML               "%s"'
                % (util.normpath('/temp/bingo.html').replace('\\', '\\\\'),
                   util.normpath('/temp/bingo2.html').replace('\\', '\\\\')))
    # hackety hack to work on win32&lin
    output = re.sub('"[c-zC-Z]:', '"', output)
    self.failUnless(output.strip() == expected)

  def testRcIncludeFile(self):
    root = grd_reader.Parse(StringIO.StringIO('''
      <includes>
        <include type="TXT" name="TEXT_ONE" file="bingo.txt"/>
        <include type="TXT" name="TEXT_TWO" file="bingo2.txt"  filenameonly="true" />
      </includes>'''), flexible_root = True)
    util.FixRootForUnittest(root, '/temp')

    buf = StringIO.StringIO()
    build.RcBuilder.ProcessNode(root, DummyOutput('rc_all', 'en'), buf)
    output = buf.getvalue()
    expected = (u'TEXT_ONE           TXT                "%s"\n'
                u'TEXT_TWO           TXT                "%s"'
                % (util.normpath('/temp/bingo.txt').replace('\\', '\\\\'),
                   'bingo2.txt'))
    # hackety hack to work on win32&lin
    output = re.sub('"[c-zC-Z]:', '"', output)
    self.failUnless(output.strip() == expected)


  def testStructureNodeOutputfile(self):
    input_file = util.PathFromRoot('grit/test/data/simple.html')
    root = grd_reader.Parse(StringIO.StringIO(
      '<structure type="tr_html" name="IDR_HTML" file="%s" />' %input_file),
      flexible_root = True)
    util.FixRootForUnittest(root, '.')
    # We must run the gatherers since we'll be wanting the translation of the
    # file.  The file exists in the location pointed to.
    root.RunGatherers(recursive=True)

    output_dir = tempfile.gettempdir()
    en_file = root.FileForLanguage('en', output_dir)
    self.failUnless(en_file == input_file)
    fr_file = root.FileForLanguage('fr', output_dir)
    self.failUnless(fr_file == os.path.join(output_dir, 'fr_simple.html'))

    fo = file(fr_file)
    contents = fo.read()
    fo.close()

    self.failUnless(contents.find('<p>') != -1)  # should contain the markup
    self.failUnless(contents.find('Hello!') == -1)  # should be translated


  def testFallbackToEnglish(self):
    root = grd_reader.Parse(StringIO.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <release seq="1" allow_pseudo="False">
          <structures fallback_to_english="True">
            <structure type="dialog" name="IDD_ABOUTBOX" file="grit\\test\data\klonk.rc" encoding="utf-16" />
          </structures>
        </release>
      </grit>'''), util.PathFromRoot('.'))
    util.FixRootForUnittest(root)
    root.RunGatherers(recursive = True)

    node = root.GetNodeById("IDD_ABOUTBOX")
    formatter = node.ItemFormatter('rc_all')
    output = formatter.Format(node, 'bingobongo')
    self.failUnless(output.strip() == '''IDD_ABOUTBOX DIALOGEX 22, 17, 230, 75
STYLE DS_SETFONT | DS_MODALFRAME | WS_CAPTION | WS_SYSMENU
CAPTION "About"
FONT 8, "System", 0, 0, 0x0
BEGIN
    ICON            IDI_KLONK,IDC_MYICON,14,9,20,20
    LTEXT           "klonk Version ""yibbee"" 1.0",IDC_STATIC,49,10,119,8,
                    SS_NOPREFIX
    LTEXT           "Copyright (C) 2005",IDC_STATIC,49,20,119,8
    DEFPUSHBUTTON   "OK",IDOK,195,6,30,11,WS_GROUP
    CONTROL         "Jack ""Black"" Daniels",IDC_RADIO1,"Button",
                    BS_AUTORADIOBUTTON,46,51,84,10
END''')


  def testRelativePath(self):
    ''' Verify that _MakeRelativePath works in some tricky cases.'''
    def TestRelativePathCombinations(base_path, other_path, expected_result):
      ''' Verify that the relative path function works for
      the given paths regardless of whether or not they end with
      a trailing slash.'''
      for path1 in [base_path, base_path + os.path.sep]:
        for path2 in [other_path, other_path + os.path.sep]:
          result = rc._MakeRelativePath(path1, path2)
          self.failUnless(result == expected_result)

    # set-up variables
    root_dir = 'c:%sa' % os.path.sep
    result1 = '..%sabc' % os.path.sep
    path1 = root_dir + 'bc'
    result2 = 'bc'
    path2 = '%s%s%s' % (root_dir, os.path.sep, result2)
    # run the tests
    TestRelativePathCombinations(root_dir, path1, result1)
    TestRelativePathCombinations(root_dir, path2, result2)


if __name__ == '__main__':
  unittest.main()

