##############################################################################
#
# Copyright (c) 2004 Zope Corporation and Contributors.
# All Rights Reserved.
#
# This software is subject to the provisions of the Zope Public License,
# Version 2.1 (ZPL).  A copy of the ZPL should accompany this distribution.
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY AND ALL EXPRESS OR IMPLIED
# WARRANTIES ARE DISCLAIMED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF TITLE, MERCHANTABILITY, AGAINST INFRINGEMENT, AND FITNESS
# FOR A PARTICULAR PURPOSE.
#
##############################################################################
"""Tests for the testing framework.

$Id: tests.py 30492 2005-05-24 20:28:53Z jim $
"""
import os
import sys
import unittest
from zope.testing import doctest, testrunner

def test_suite():
    return unittest.TestSuite((
        doctest.DocTestSuite('zope.testing.renormalizing'),
        doctest.DocFileSuite('formparser.txt'),
        doctest.DocTestSuite('zope.testing.loggingsupport'),
        testrunner.test_suite(),
        ))

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
