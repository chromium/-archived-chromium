#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for classes in gtest_command.py."""

import unittest

from log_parser import gtest_command

RELOAD_ERRORS = """
C:\b\slave\chrome-release-snappy\build\chrome\browser\navigation_controller_unittest.cc:381: Failure
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

"""

BACK_ERRORS = """
C:\b\slave\chrome-release-snappy\build\chrome\browser\navigation_controller_unittest.cc:439: Failure
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

"""

SWITCH_ERRORS = """
C:\b\slave\chrome-release-snappy\build\chrome\browser\navigation_controller_unittest.cc:615: Failure
Value of: -1
Expected: contents->controller()->GetPendingEntryIndex()
Which is: 0

C:\b\slave\chrome-release-snappy\build\chrome\browser\navigation_controller_unittest.cc:617: Failure
Value of: contents->controller()->GetPendingEntry()
  Actual: true
Expected: false

"""

TEST_DATA = """
Running 1 test from test case PageLoadTrackerUnitTest . . .
  Test AddMetrics running . . .
  Test AddMetrics passed.
Test case PageLoadTrackerUnitTest passed.

Running 13 tests from test case NavigationControllerTest . . .
  Test Defaults running . . .
  Test Defaults passed.
  Test Reload running . . .
%(reload_errors)s
  Test Reload failed.
  Test Reload_GeneratesNewPage running . . .
  Test Reload_GeneratesNewPage passed.
  Test Back running . . .
%(back_errors)s
  Test Back failed.
Test case NavigationControllerTest failed.

Running 2 tests from test case SomeOtherTest . . .
  Test SwitchTypes running . . .
%(switch_errors)s
  Test SwitchTypes failed.
Test case SomeOtherTest failed.
""" % {'reload_errors': RELOAD_ERRORS,
       'back_errors'  : BACK_ERRORS,
       'switch_errors': SWITCH_ERRORS}

class TestObserverTests(unittest.TestCase):
  def testLogLineObserver(self):
    observer = gtest_command.TestObserver()
    for line in TEST_DATA.splitlines():
      observer.outLineReceived(line)

    test_name = 'NavigationControllerTest.Reload'
    self.assertEqual('\n'.join(['%s:' % test_name, RELOAD_ERRORS]),
                     '\n'.join(observer.failed_tests[test_name]))

    test_name = 'NavigationControllerTest.Back'
    self.assertEqual('\n'.join(['%s:' % test_name, BACK_ERRORS]),
                     '\n'.join(observer.failed_tests[test_name]))

    test_name = 'SomeOtherTest.SwitchTypes'
    self.assertEqual('\n'.join(['%s:' % test_name, SWITCH_ERRORS]),
                     '\n'.join(observer.failed_tests[test_name]))


if __name__ == '__main__':
  unittest.main()
