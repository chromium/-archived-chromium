# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper class for reading in and dealing with tests expectations
for layout tests.
"""

import os
import re
import sys
import path_utils
import compare_failures


# Test expectation constants.
PASS    = 0
FAIL    = 1
TIMEOUT = 2
CRASH   = 3


class TestExpectations:
  FIXABLE = "tests_fixable.txt"
  IGNORED = "tests_ignored.txt"

  def __init__(self, tests, directory, platform, is_debug_mode):
    """Reads the test expectations files from the given directory."""
    self._tests = tests
    self._directory = directory
    self._platform = platform
    self._is_debug_mode = is_debug_mode
    self._ReadFiles()
    self._ValidateLists()

  def GetFixable(self):
    return (self._fixable.GetTests() -
            self._fixable.GetNonSkippedDeferred() -	 
            self._fixable.GetSkippedDeferred())

  def GetFixableSkipped(self):
    return self._fixable.GetSkipped()

  def GetFixableSkippedDeferred(self):	 
    return self._fixable.GetSkippedDeferred()

  def GetFixableFailures(self):
    return (self._fixable.GetTestsExpectedTo(FAIL) -
            self._fixable.GetTestsExpectedTo(TIMEOUT) -
            self._fixable.GetTestsExpectedTo(CRASH) -
            self._fixable.GetNonSkippedDeferred())

  def GetFixableTimeouts(self):
    return (self._fixable.GetTestsExpectedTo(TIMEOUT) -
            self._fixable.GetTestsExpectedTo(CRASH) -
            self._fixable.GetNonSkippedDeferred())

  def GetFixableCrashes(self):
    return self._fixable.GetTestsExpectedTo(CRASH)

  def GetFixableDeferred(self):	 
    return self._fixable.GetNonSkippedDeferred()	 

  def GetFixableDeferredFailures(self):	 
    return (self._fixable.GetNonSkippedDeferred() &	 
            self._fixable.GetTestsExpectedTo(FAIL))	 

  def GetFixableDeferredTimeouts(self):	 
    return (self._fixable.GetNonSkippedDeferred() &	 
            self._fixable.GetTestsExpectedTo(TIMEOUT))

  def GetIgnored(self):
    return self._ignored.GetTests()

  def GetIgnoredSkipped(self):
    return self._ignored.GetSkipped()

  def GetIgnoredFailures(self):
    return (self._ignored.GetTestsExpectedTo(FAIL) -
            self._ignored.GetTestsExpectedTo(TIMEOUT))

  def GetIgnoredTimeouts(self):
    return self._ignored.GetTestsExpectedTo(TIMEOUT)

  def GetExpectations(self, test):
    if self._fixable.Contains(test): return self._fixable.GetExpectations(test)
    if self._ignored.Contains(test): return self._ignored.GetExpectations(test)
    # If the test file is not listed in any of the expectations lists
    # we expect it to pass (and nothing else).
    return set([PASS])

  def IsFixable(self, test):
    return (self._fixable.Contains(test) and
            test not in self._fixable.GetNonSkippedDeferred())

  def IsIgnored(self, test):
    return (self._ignored.Contains(test) and
            test not in self._fixable.GetNonSkippedDeferred())

  def _ReadFiles(self):
    self._fixable = self._GetExpectationsFile(self.FIXABLE)
    self._ignored = self._GetExpectationsFile(self.IGNORED)
    skipped = self.GetFixableSkipped() | self.GetIgnoredSkipped()
    self._fixable.PruneSkipped(skipped)
    self._ignored.PruneSkipped(skipped)

  def _GetExpectationsFile(self, filename):
    """Read the expectation files for the given filename and return a single
    expectations file with the merged results.
    """
    
    path = os.path.join(self._directory, filename)
    return TestExpectationsFile(path, self._tests, self._platform,
        self._is_debug_mode)

  def _ValidateLists(self):
    # Make sure there's no overlap between the tests in the two files.
    overlap = self._fixable.GetTests() & self._ignored.GetTests()
    message = "Files contained in both " + self.FIXABLE + " and " + self.IGNORED
    compare_failures.PrintFilesFromSet(overlap, message, sys.stdout)
    assert(len(overlap) == 0)
    # Make sure there are no ignored tests expected to crash.
    assert(len(self._ignored.GetTestsExpectedTo(CRASH)) == 0)


def StripComments(line):
  """Strips comments from a line and return None if the line is empty
  or else the contents of line with leading and trailing spaces removed
  and all other whitespace collapsed"""
  
  commentIndex = line.find('//')
  if commentIndex is -1:
    commentIndex = len(line)
    
  line = re.sub(r'\s+', ' ', line[:commentIndex].strip())
  if line == '': return None
  else: return line

class TestExpectationsFile:
  """Test expectation files consist of lines with specifications of what
  to expect from layout test cases. The test cases can be directories
  in which case the expectations apply to all test cases in that
  directory and any subdirectory. The format of the file is along the
  lines of:
  
    LayoutTests/fast/js/fixme.js = FAIL
    LayoutTests/fast/js/flaky.js = FAIL PASS
    LayoutTests/fast/js/crash.js = CRASH TIMEOUT FAIL PASS
    ...

  To add other options:
    SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
    DEBUG : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
    DEBUG SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
    LINUX DEBUG SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
    DEFER LINUX WIN : LayoutTests/fast/js/no-good.js = TIMEOUT PASS

  SKIP: Doesn't run the test.
  DEFER: Test does not count in our statistics for the current release.
  DEBUG: Expectations apply only to the debug build.
  RELEASE: Expectations apply only to release build.
  LINUX/WIN/MAC: Expectations apply only to these platforms.

  A test can be included twice, but not via the same path. If a test is included
  twice, then the more precise path wins.
  """

  EXPECTATIONS = { 'pass': PASS,
                   'fail': FAIL,
                   'timeout': TIMEOUT,
                   'crash': CRASH }
  
  PLATFORMS = [ 'mac', 'linux', 'win' ]

  def __init__(self, path, full_test_list, platform, is_debug_mode):
    """
    path: The path to the expectation file. An error is thrown if a test is
        listed more than once. 
    full_test_list: The list of all tests to be run pending processing of the
        expections for those tests.
    platform: Which platform from self.PLATFORMS to filter tests for.
    is_debug_mode: Whether we testing a test_shell built debug mode.
    """
    
    self._full_test_list = full_test_list
    self._skipped = set()
    self._skipped_deferred = set()	 
    self._non_skipped_deferred = set()
    self._expectations = {}
    self._test_list_paths = {}
    self._tests = {}
    self._errors = []
    self._platform = platform
    self._is_debug_mode = is_debug_mode
    for expectation in self.EXPECTATIONS.itervalues():
      self._tests[expectation] = set()
    self._Read(path)

  def GetSkipped(self):
    return self._skipped

  def GetNonSkippedDeferred(self):	 
    return self._non_skipped_deferred	 
 	 
  def GetSkippedDeferred(self):	 
    return self._skipped_deferred

  def GetExpectations(self, test):
    return self._expectations[test]

  def GetTests(self):
    return set(self._expectations.keys())

  def GetTestsExpectedTo(self, expectation):
    return self._tests[expectation]

  def Contains(self, test):
    return test in self._expectations

  def PruneSkipped(self, skipped):
    for test in skipped:
      if not test in self._expectations: continue
      for expectation in self._expectations[test]:
        self._tests[expectation].remove(test)
      del self._expectations[test]

  def _HasCurrentPlatform(self, options):
    """ Returns true if the current platform is in the options list or if no
    platforms are listed.
    """
    has_any_platforms = False
    
    for platform in self.PLATFORMS:
      if platform in options:
        has_any_platforms = True
        break

    if not has_any_platforms:
      return True

    return self._platform in options

  def _Read(self, path):
    """For each test in an expectations file, generate the expectations for it.

    """

    lineno = 0
    for line in open(path):
      lineno += 1
      line = StripComments(line)
      if not line: continue

      if line.find(':') is -1:
        test_and_expectations = line
        is_skipped = False
        is_deferred = False
      else:
        parts = line.split(':')
        test_and_expectations = parts[1]
        options = self._GetOptionsList(parts[0])
        is_skipped = 'skip' in options
        is_deferred = 'defer' in options
        if 'release' in options or 'debug' in options:
          if self._is_debug_mode and 'debug' not in options:
            continue
          if not self._is_debug_mode and 'release' not in options:
            continue
        if not self._HasCurrentPlatform(options):
          continue

      tests_and_expecation_parts = test_and_expectations.split('=')
      if (len(tests_and_expecation_parts) is not 2):
        self._AddError(lineno, 'Missing expectations.', test_and_expectations)
        continue

      test_list_path = tests_and_expecation_parts[0].strip()
      try:
        expectations = self._ParseExpectations(tests_and_expecation_parts[1])
      except SyntaxError, err:
        self._AddError(lineno, err[0], test_list_path)
        continue

      full_path = os.path.join(path_utils.LayoutDataDir(), test_list_path)
      full_path = os.path.normpath(full_path)
      # WebKit's way of skipping tests is to add a -disabled suffix.
      # So we should consider the path existing if the path or the -disabled
      # version exists.
      if not os.path.exists(full_path) and not \
        os.path.exists(full_path + '-disabled'):
        self._AddError(lineno, 'Path does not exist.', test_list_path)
        continue

      if not self._full_test_list:
        tests = [test_list_path]
      else:
        tests = self._ExpandTests(test_list_path)

      if is_skipped:    
        self._AddSkippedTests(tests, is_deferred)
      else:
        self._AddTests(tests, expectations, test_list_path, lineno,
            is_deferred)

    if len(self._errors) is not 0:
      print "\nFAILURES FOR PLATFORM: %s, IS_DEBUG_MODE: %s" \
          % (self._platform.upper(), self._is_debug_mode)        
      raise SyntaxError('\n'.join(map(str, self._errors)))

  def _GetOptionsList(self, listString):
    return [part.strip().lower() for part in listString.strip().split(' ')]

  def _ParseExpectations(self, string):
    result = set()
    for part in self._GetOptionsList(string):
      if not part in self.EXPECTATIONS:
        raise SyntaxError('Unsupported expectation: ' + part)
      expectation = self.EXPECTATIONS[part]
      result.add(expectation)
    return result

  def _ExpandTests(self, test_list_path):
    # Convert the test specification to an absolute, normalized
    # path and make sure directories end with the OS path separator.
    path = os.path.join(path_utils.LayoutDataDir(), test_list_path)
    path = os.path.normpath(path)
    if os.path.isdir(path): path = os.path.join(path, '')
    # This is kind of slow - O(n*m) - since this is called for all 
    # entries in the test lists. It has not been a performance 
    # issue so far. Maybe we should re-measure the time spent reading
    # in the test lists?
    result = []
    for test in self._full_test_list:
      if test.startswith(path): result.append(test)
    return result

  def _AddTests(self, tests, expectations, test_list_path, lineno,
      is_deferred):
    for test in tests:
      if test in self._test_list_paths:
        prev_base_path = self._test_list_paths[test]
        if (prev_base_path == os.path.normpath(test_list_path)):
          self._AddError(lineno, 'Duplicate expecations.', test)
          continue
        if prev_base_path.startswith(test_list_path):
          # already seen a more precise path
          continue

      # Remove prexisiting expectations for this test.
      if test in self._test_list_paths:
        for expectation in self.EXPECTATIONS.itervalues():
          if test in self._tests[expectation]:
            self._tests[expectation].remove(test)

      # Now add the new expectations.
      self._expectations[test] = expectations
      self._test_list_paths[test] = os.path.normpath(test_list_path)

      if is_deferred:	 
        self._non_skipped_deferred.add(test)

      for expectation in expectations:
        if expectation == CRASH and is_deferred:
          self._AddError(lineno, 'Crashes cannot be deferred.', test)
        self._tests[expectation].add(test)
 
  def _AddSkippedTests(self, tests, is_deferred):
    for test in tests:
      if is_deferred:	 
        self._skipped_deferred.add(test)
      self._skipped.add(test)

  def _AddError(self, lineno, msg, path):
    self._errors.append('\nLine:%s %s\n%s' % (lineno, msg, path))