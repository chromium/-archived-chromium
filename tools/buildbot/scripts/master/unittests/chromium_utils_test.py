#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Source file for chromium_utils testcases."""

import unittest
import chromium_utils

class PartiallyInitializeTest(unittest.TestCase):

  def testConstructorCurring(self):
    class PartiallyConstructable:
      def __init__(self, arg1, named_arg1=None, named_arg2=None):
        self.arg1 = arg1
        self.named_arg1 = named_arg1
        self.named_arg2 = named_arg2

    partial_class = chromium_utils.InitializePartiallyWithArguments(
        PartiallyConstructable, 'argument 1 value',
        named_arg1='named argument 1 value')
    complete_class = partial_class(named_arg2='named argument 2 value')
    self.assertEqual('argument 1 value', complete_class.arg1)
    self.assertEqual('named argument 2 value', complete_class.named_arg2)

class FilteredMeanAndStandardDeviationTest(unittest.TestCase):

  def testFilteredMeanAndStandardDeviation(self):
    sample_data = [4, 4, 6, 12345, 100, 20] # max should be ignored
    mean, stdd = chromium_utils.FilteredMeanAndStandardDeviation(sample_data)
    self.assertEqual(26.8, mean)
    self.assertAlmostEqual(37.08585, stdd, 5)
