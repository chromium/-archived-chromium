#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from log_parser import cl_command

class IbOutputParserTest(unittest.TestCase):

  ALL_PROJECT_NAMES_OK = [
     'generated_resources', 'icudt','libpng', 'base','base_unittests']

  FAILED_PROJECTS = {'libpng' : 1, 'base_unittests' : 2}

  WARNING_LINES = ['c:\libpng\pngerror.c(116) : warning C4114: warning 1',
                   'c:\string_util_unittest.cc : warning C4819: Warning 2']

  def testFoundWarningsFalse(self):
   self.assert_(not self.__parser('ok-log-compile-stdio').FoundWarnings())

  def testFoundWarningsTrue(self):
   self.assert_(self.__parser('error-log-compile-stdio').FoundWarnings())

  def testFoundErrorsFalse(self):
    self.assert_(not self.__parser('ok-log-compile-stdio').FoundErrors())

  def testFoundErrorsTrue(self):
    self.assert_(self.__parser('error-log-compile-stdio').FoundErrors())

  def testWarningLines(self):
    expected_lines = self.__parser('error-log-compile-stdio').WarningLines()
    self.assert_(IbOutputParserTest.WARNING_LINES.count(expected_lines[0]))
    self.assert_(IbOutputParserTest.WARNING_LINES.count(expected_lines[1]))

  def testFailedProjects(self):
    self.assertEqual(
        IbOutputParserTest.FAILED_PROJECTS,
        self.__parser('error-log-compile-stdio').FailedProjects())

  def testIndividualProjectErrorsAnchoredAndSkinned(self):
    expected = ("<div class='error'><a name='base_unittests_1'>c:\\builds\\"
      "chrome\\chrome-release\\build\\base\\string_util_unittest.cc : "
      "error C2220: warning treated as error - no 'object' file generated"
      "<br /></a></div>")
    found_expected_line = False
    html = self.__parser('error-log-compile-stdio').ErrorLogHtml()
    for line in html.splitlines():
      if line.strip() == expected:
        found_expected_line = True
        break
    self.assert_(found_expected_line)

  def testIndividualProjectErrorsAnchoredAndSkinnedByErrorIndex(self):
    expected = ("<div class='error'><a name='base_unittests_2'>"
    "c:\\builds\\chrome\\chrome-release\\build\\base\\time_unittest.cc "
    ": error C2220: warning treated as error - no 'object' file generated"
    "<br /></a></div>")
    found_expected_line = False
    html = self.__parser('error-log-compile-stdio').ErrorLogHtml()
    for line in html.splitlines():
      if line.strip() == expected:
        found_expected_line = True
        break
    self.assert_(found_expected_line)

  def testIsIbLog(self):
    self.assert_(self.__parser('error-log-compile-stdio').
                 _IsIbLog())

  def testIsIbLogFalse(self):
    self.assert_(not self.__parser('error-log-compile-stdio-devenv').
                 _IsIbLog())

  def __parser(self, logfile):
    file = open(os.path.join('data', logfile))
    parser = cl_command.IbOutputParser(file.read())
    file.close()
    return parser


if __name__ == '__main__':
  unittest.main()