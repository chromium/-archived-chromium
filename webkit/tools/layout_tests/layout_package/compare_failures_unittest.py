#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests to make sure we generate and update the expected-*.txt files
properly after running layout tests."""

import os
import shutil
import tempfile
import unittest

import compare_failures
import path_utils
import test_failures

class CompareFailuresUnittest(unittest.TestCase):
  def setUp(self):
    """Makes a temporary results directory and puts expected-failures.txt and
    expected-crashes.txt into it."""
    self._tempdir = tempfile.mkdtemp()
    # copy over expected-*.txt files
    testdatadir = self.GetTestDataDir()
    filenames = ("expected-passing.txt", "expected-failures.txt",
                 "expected-crashes.txt")
    for filename in filenames:
      # copyfile doesn't copy file permissions so we can delete the files later
      shutil.copyfile(os.path.join(testdatadir, filename),
                      os.path.join(self._tempdir, filename))

  def tearDown(self):
    """Remove temp directory."""
    shutil.rmtree(self._tempdir)
    self._tempdir = None

  ###########################################################################
  # Tests
  def testGenerateNewBaseline(self):
    """Test the generation of new expected-*.txt files when either they don't
    exist or the user explicitly asks to make new files."""
    failures = self.GetTestFailures()

    # Test to make sure we generate baseline files if the file doesn't exist.
    os.remove(os.path.join(self.GetTmpDir(), 'expected-passing.txt'))
    os.remove(os.path.join(self.GetTmpDir(), 'expected-failures.txt'))
    os.remove(os.path.join(self.GetTmpDir(), 'expected-crashes.txt'))

    # Test force generation of new baseline files with a new failure and one
    # less passing.
    pass_file = os.path.join(path_utils.LayoutDataDir(), 'fast', 'pass1.html')
    failures[pass_file] = [test_failures.FailureTextMismatch(None)]

    cf = compare_failures.CompareFailures(self.GetTestFiles(), failures,
                                          set(), set(),
                                          self.GetTmpDir(), False)
    cf.UpdateFailuresOnDisk()
    self.CheckOutputWithExpectedFiles('expected-passing-new-baseline.txt',
                                      'expected-failures-added.txt',
                                      'expected-crashes.txt')

  def testPassingToFailure(self):
    """When there's a new failure, we don't add it to the baseline."""
    failures = self.GetTestFailures()

    # Test case where we don't update new baseline.  We have a new failure,
    # but it shouldn't be added to the expected-failures.txt file.
    pass_file = os.path.join(path_utils.LayoutDataDir(), 'fast', 'pass1.html')
    failures[pass_file] = [test_failures.FailureTextMismatch(None)]
    self.CheckNoChanges(failures)

    # Same thing as before: pass -> crash
    failures[pass_file] = [test_failures.FailureCrash()]
    self.CheckNoChanges(failures)

  def testFailureToCrash(self):
    """When there's a new crash, we don't add it to the baseline or remove it
    from the failure list."""
    failures = self.GetTestFailures()

    # Test case where we don't update new baseline.  A failure moving to a
    # crash shouldn't be added to the expected-crashes.txt file.
    failure_file = os.path.join(path_utils.LayoutDataDir(),
                                'fast', 'foo', 'fail1.html')
    failures[failure_file] = [test_failures.FailureCrash()]
    self.CheckNoChanges(failures)

  def testFailureToPassing(self):
    """This is better than before, so we should update the failure list."""
    failures = self.GetTestFailures()

    # Remove one of the failing test cases from the failures dictionary.  This
    # makes failure_file considered to be passing.
    failure_file = os.path.join(path_utils.LayoutDataDir(),
                                'fast', 'bar', 'fail2.html')
    del failures[failure_file]

    cf = compare_failures.CompareFailures(self.GetTestFiles(), failures,
                                          set(), set(),
                                          self.GetTmpDir(), False)
    cf.UpdateFailuresOnDisk()
    self.CheckOutputWithExpectedFiles('expected-passing-new-passing2.txt',
                                      'expected-failures-new-passing.txt',
                                      'expected-crashes.txt')

  def testCrashToPassing(self):
    """This is better than before, so we update the crashes file."""
    failures = self.GetTestFailures()

    crash_file = os.path.join(path_utils.LayoutDataDir(),
                              'fast', 'bar', 'betz', 'crash3.html')
    del failures[crash_file]
    cf = compare_failures.CompareFailures(self.GetTestFiles(), failures,
                                          set(), set(),
                                          self.GetTmpDir(), False)
    cf.UpdateFailuresOnDisk()
    self.CheckOutputWithExpectedFiles('expected-passing-new-passing.txt',
                                      'expected-failures.txt',
                                      'expected-crashes-new-passing.txt')

  def testCrashToFailure(self):
    """This is better than before, so we should update both lists."""
    failures = self.GetTestFailures()

    crash_file = os.path.join(path_utils.LayoutDataDir(),
                              'fast', 'bar', 'betz', 'crash3.html')
    failures[crash_file] = [test_failures.FailureTextMismatch(None)]
    cf = compare_failures.CompareFailures(self.GetTestFiles(), failures,
                                          set(), set(),
                                          self.GetTmpDir(), False)
    cf.UpdateFailuresOnDisk()
    self.CheckOutputWithExpectedFiles('expected-passing.txt',
                                      'expected-failures-new-crash.txt',
                                      'expected-crashes-new-passing.txt')

  def testNewTestPass(self):
    """After a merge, we need to update new passing tests properly."""
    files = self.GetTestFiles()
    new_test_file = os.path.join(path_utils.LayoutDataDir(), "new-test.html")
    files.add(new_test_file)
    failures = self.GetTestFailures()

    # New test file passing
    cf = compare_failures.CompareFailures(files, failures, set(), set(),
                                          self.GetTmpDir(), False)
    cf.UpdateFailuresOnDisk()
    self.CheckOutputWithExpectedFiles('expected-passing-new-test.txt',
                                      'expected-failures.txt',
                                      'expected-crashes.txt')

  def testNewTestFail(self):
    """After a merge, we need to update new failing tests properly."""
    files = self.GetTestFiles()
    new_test_file = os.path.join(path_utils.LayoutDataDir(), "new-test.html")
    files.add(new_test_file)
    failures = self.GetTestFailures()

    # New test file failing
    failures[new_test_file] = [test_failures.FailureTextMismatch(None)]
    cf = compare_failures.CompareFailures(files, failures, set(), set(),
                                          self.GetTmpDir(), False)
    cf.UpdateFailuresOnDisk()
    self.CheckOutputWithExpectedFiles('expected-passing.txt',
                                      'expected-failures-new-test.txt',
                                      'expected-crashes.txt')

  def testNewTestCrash(self):
    """After a merge, we need to update new crashing tests properly."""
    files = self.GetTestFiles()
    new_test_file = os.path.join(path_utils.LayoutDataDir(), "new-test.html")
    files.add(new_test_file)
    failures = self.GetTestFailures()

    # New test file crashing
    failures[new_test_file] = [test_failures.FailureCrash()]
    cf = compare_failures.CompareFailures(files, failures, set(), set(),
                                          self.GetTmpDir(), False)
    cf.UpdateFailuresOnDisk()
    self.CheckOutputWithExpectedFiles('expected-passing.txt',
                                      'expected-failures.txt',
                                      'expected-crashes-new-test.txt')

  def testHasNewFailures(self):
    files = self.GetTestFiles()
    failures = self.GetTestFailures()

    # no changes, no new failures
    cf = compare_failures.CompareFailures(files, failures, set(), set(),
                                          self.GetTmpDir(), False)
    self.failUnless(not cf.HasNewFailures())

    # test goes from passing to failing
    pass_file = os.path.join(path_utils.LayoutDataDir(), 'fast', 'pass1.html')
    failures[pass_file] = [test_failures.FailureTextMismatch(None)]
    cf = compare_failures.CompareFailures(files, failures, set(), set(),
                                          self.GetTmpDir(), False)
    self.failUnless(cf.HasNewFailures())

    # Failing to passing
    failures = self.GetTestFailures()
    failure_file = os.path.join(path_utils.LayoutDataDir(),
                                'fast', 'bar', 'fail2.html')
    del failures[failure_file]
    cf = compare_failures.CompareFailures(files, failures, set(), set(),
                                          self.GetTmpDir(), False)
    self.failUnless(not cf.HasNewFailures())

    # A new test that fails, this doesn't count as a new failure.
    new_test_file = os.path.join(path_utils.LayoutDataDir(), "new-test.html")
    files.add(new_test_file)
    failures = self.GetTestFailures()
    failures[new_test_file] = [test_failures.FailureCrash()]
    cf = compare_failures.CompareFailures(files, failures, set(), set(),
                                          self.GetTmpDir(), False)
    self.failUnless(not cf.HasNewFailures())


  ###########################################################################
  # Helper methods
  def CheckOutputEqualsExpectedFile(self, output, expected):
    """Compares a file in our output dir against a file from the testdata
    directory."""
    output = os.path.join(self.GetTmpDir(), output)
    expected = os.path.join(self.GetTestDataDir(), expected)
    self.failUnlessEqual(open(output).read(), open(expected).read())

  def CheckOutputWithExpectedFiles(self, passing, failing, crashing):
    """Compare all three output files against three provided expected
    files."""
    self.CheckOutputEqualsExpectedFile('expected-passing.txt', passing)
    self.CheckOutputEqualsExpectedFile('expected-failures.txt', failing)
    self.CheckOutputEqualsExpectedFile('expected-crashes.txt', crashing)

  def CheckNoChanges(self, failures):
    """Verify that none of the expected-*.txt files have changed."""
    cf = compare_failures.CompareFailures(self.GetTestFiles(), failures,
                                          set(), set(),
                                          self.GetTmpDir(), False)
    cf.UpdateFailuresOnDisk()
    self.CheckOutputWithExpectedFiles('expected-passing.txt',
                                      'expected-failures.txt',
                                      'expected-crashes.txt')

  def GetTestDataDir(self):
    return os.path.abspath('testdata')

  def GetTmpDir(self):
    return self._tempdir

  def GetTestFiles(self):
    """Get a set of files that includes the expected crashes and failures
    along with two passing tests."""
    layout_dir = path_utils.LayoutDataDir()
    files = [
      'fast\\pass1.html',
      'fast\\foo\\pass2.html',
      'fast\\foo\\crash1.html',
      'fast\\bar\\crash2.html',
      'fast\\bar\\betz\\crash3.html',
      'fast\\foo\\fail1.html',
      'fast\\bar\\fail2.html',
      'fast\\bar\\betz\\fail3.html',
    ]

    return set([os.path.join(layout_dir, f) for f in files])

  def GetTestFailures(self):
    """Get a dictionary representing the crashes and failures in the
    expected-*.txt files."""
    failures = {}
    for filename in self.GetTestFiles():
      if filename.find('crash') != -1:
        failures[filename] = [test_failures.FailureCrash()]
      elif filename.find('fail') != -1:
        failures[filename] = [test_failures.FailureTextMismatch(None)]

    return failures

if '__main__' == __name__:
  unittest.main()

