#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test that checks some of util functions.
'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import unittest

from grit import util


class UtilUnittest(unittest.TestCase):
  ''' Tests functions from util
  '''

  def testNewClassInstance(self):
    # Test short class name with no fully qualified package name
    # Should fail, it is not supported by the function now (as documented)
    cls = util.NewClassInstance('grit.util.TestClassToLoad',
                                TestBaseClassToLoad)
    self.failUnless(cls == None)

    # Test non existent class name
    cls = util.NewClassInstance('grit.util_unittest.NotExistingClass',
                                TestBaseClassToLoad)
    self.failUnless(cls == None)

    # Test valid class name and valid base class
    cls = util.NewClassInstance('grit.util_unittest.TestClassToLoad',
                                TestBaseClassToLoad)
    self.failUnless(isinstance(cls, TestBaseClassToLoad))

    # Test valid class name with wrong hierarchy
    cls = util.NewClassInstance('grit.util_unittest.TestClassNoBase',
                                TestBaseClassToLoad)
    self.failUnless(cls == None)

  def testCanonicalLanguage(self):
    self.failUnless(util.CanonicalLanguage('en') == 'en')
    self.failUnless(util.CanonicalLanguage('pt_br') == 'pt-BR')
    self.failUnless(util.CanonicalLanguage('pt-br') == 'pt-BR')
    self.failUnless(util.CanonicalLanguage('pt-BR') == 'pt-BR')
    self.failUnless(util.CanonicalLanguage('pt/br') == 'pt-BR')
    self.failUnless(util.CanonicalLanguage('pt/BR') == 'pt-BR')
    self.failUnless(util.CanonicalLanguage('no_no_bokmal') == 'no-NO-BOKMAL')

  def testUnescapeHtml(self):
    self.failUnless(util.UnescapeHtml('&#1010;') == unichr(1010))
    self.failUnless(util.UnescapeHtml('&#xABcd;') == unichr(43981))

class TestBaseClassToLoad(object):
  pass

class TestClassToLoad(TestBaseClassToLoad):
  pass

class TestClassNoBase(object):
  pass


if __name__ == '__main__':
  unittest.main()

