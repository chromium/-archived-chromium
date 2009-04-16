# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper class for reading in and dealing with tests expectations
for layout tests.
"""

import logging
import os
import re
import sys
import path_utils
import compare_failures


# Test expectation and modifier constants.
(PASS, FAIL, TIMEOUT, CRASH, SKIP, WONTFIX, DEFER, SLOW, NONE) = range(9)

class TestExpectations:
  TEST_LIST = "test_expectations.txt"

  def __init__(self, tests, directory, platform, is_debug_mode):
    """Reads the test expectations files from the given directory."""
    path = os.path.join(directory, self.TEST_LIST)
    self._expected_failures = TestExpectationsFile(path, tests, platform,
        is_debug_mode)

  # TODO(ojan): Allow for removing skipped tests when getting the list of tests
  # to run, but not when getting metrics.
  # TODO(ojan): Replace the Get* calls here with the more sane API exposed
  # by TestExpectationsFile below. Maybe merge the two classes entirely?

  def GetFixable(self):
    return self._expected_failures.GetTestSet(NONE)

  def GetFixableFailures(self):
    # Avoid including TIMEOUT CRASH FAIL tests in the fail numbers since
    # crashes and timeouts are higher priority.
    return (self._expected_failures.GetTestSet(NONE, FAIL) -
            self._expected_failures.GetTestSet(NONE, TIMEOUT) -
            self._expected_failures.GetTestSet(NONE, CRASH))

  def GetFixableTimeouts(self):
    # Avoid including TIMEOUT CRASH tests in the timeout numbers since crashes
    # are higher priority.
    return (self._expected_failures.GetTestSet(NONE, TIMEOUT) -
            self._expected_failures.GetTestSet(NONE, CRASH))

  def GetFixableCrashes(self):
    return self._expected_failures.GetTestSet(NONE, CRASH)

  def GetSkipped(self):
    # TODO(ojan): It's confusing that this includes deferred tests.
    return (self._expected_failures.GetTestSet(SKIP) -
            self._expected_failures.GetTestSet(WONTFIX))

  def GetDeferred(self):
    return self._expected_failures.GetTestSet(DEFER, include_skips=False)

  def GetDeferredSkipped(self):
    return (self._expected_failures.GetTestSet(SKIP) &
            self._expected_failures.GetTestSet(DEFER))

  def GetDeferredFailures(self):
    return self._expected_failures.GetTestSet(DEFER, FAIL,
        include_skips=False)

  def GetDeferredTimeouts(self):
    return self._expected_failures.GetTestSet(DEFER, TIMEOUT,
        include_skips=False)

  def GetWontFix(self):
    return self._expected_failures.GetTestSet(WONTFIX, include_skips=False)

  def GetWontFixSkipped(self):
    return (self._expected_failures.GetTestSet(WONTFIX) &
            self._expected_failures.GetTestSet(SKIP))

  def GetWontFixFailures(self):
    # Avoid including TIMEOUT CRASH FAIL tests in the fail numbers since
    # crashes and timeouts are higher priority.
    return (self._expected_failures.GetTestSet(WONTFIX, FAIL,
                include_skips=False) -
            self._expected_failures.GetTestSet(WONTFIX, TIMEOUT,
                include_skips=False) -
            self._expected_failures.GetTestSet(WONTFIX, CRASH,
                include_skips=False))

  def GetWontFixTimeouts(self):
    # Avoid including TIMEOUT CRASH tests in the timeout numbers since crashes
    # are higher priority.
    return (self._expected_failures.GetTestSet(WONTFIX, TIMEOUT,
                include_skips=False) -
            self._expected_failures.GetTestSet(WONTFIX, CRASH,
                include_skips=False))

  def GetExpectations(self, test):
    if self._expected_failures.Contains(test):
      return self._expected_failures.GetExpectations(test)

    # If the test file is not listed in any of the expectations lists
    # we expect it to pass (and nothing else).
    return set([PASS])

  def IsDeferred(self, test):
    return self._expected_failures.HasModifier(test, DEFER)

  def IsFixable(self, test):
    return self._expected_failures.HasModifier(test, NONE)

  def IsIgnored(self, test):
    return self._expected_failures.HasModifier(test, WONTFIX)

  def HasModifier(self, test, modifier):
    return self._expected_failures.HasModifier(test, modifier)

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
  SLOW: The test takes a long time to run, but does not timeout indefinitely.
  WONTFIX: For tests that we never intend to pass on a given platform.
  DEFER: Test does not count in our statistics for the current release.
  DEBUG: Expectations apply only to the debug build.
  RELEASE: Expectations apply only to release build.
  LINUX/WIN/MAC: Expectations apply only to these platforms.

  Notes:
    -A test cannot be both SLOW and TIMEOUT
    -A test cannot be both DEFER and WONTFIX
    -A test can be included twice, but not via the same path.
    -If a test is included twice, then the more precise path wins.
    -CRASH tests cannot be DEFER or WONTFIX
  """

  EXPECTATIONS = { 'pass': PASS,
                   'fail': FAIL,
                   'timeout': TIMEOUT,
                   'crash': CRASH }

  PLATFORMS = [ 'mac', 'linux', 'win' ]
  
  BUILD_TYPES = [ 'debug', 'release' ]

  MODIFIERS = { 'skip': SKIP,
                'wontfix': WONTFIX,
                'defer': DEFER,
                'slow': SLOW,
                'none': NONE }

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
    self._errors = []
    self._non_fatal_errors = []
    self._platform = platform
    self._is_debug_mode = is_debug_mode
    
    # Maps a test to its list of expectations.
    self._test_to_expectations = {}

    # Maps a test to the base path that it was listed with in the test list.
    self._test_list_paths = {}

    # Maps a modifier to a set of tests.
    self._modifier_to_tests = {}
    for modifier in self.MODIFIERS.itervalues():
      self._modifier_to_tests[modifier] = set()

    # Maps an expectation to a set of tests.
    self._expectation_to_tests = {}
    for expectation in self.EXPECTATIONS.itervalues():
      self._expectation_to_tests[expectation] = set()

    self._Read(path)

  def GetTestSet(self, modifier, expectation=None, include_skips=True):
    if expectation is None:
      tests = self._modifier_to_tests[modifier]
    else:
      tests = (self._expectation_to_tests[expectation] &
          self._modifier_to_tests[modifier])
    
    if not include_skips:
      tests = tests - self.GetTestSet(SKIP, expectation)
    
    return tests

  def HasModifier(self, test, modifier):
    return test in self._modifier_to_tests[modifier]

  def GetExpectations(self, test):
    return self._test_to_expectations[test]

  def Contains(self, test):
    return test in self._test_to_expectations

  def _HasValidModifiersForCurrentPlatform(self, options, lineno,
      test_and_expectations, modifiers):
    """ Returns true if the current platform is in the options list or if no
    platforms are listed and if there are no fatal errors in the options list.

    Args:
      options: List of lowercase options.
      lineno: The line in the file where the test is listed.
      test_and_expectations: The path and expectations for the test.
      modifiers: The set to populate with modifiers.
    """
    has_any_platform = False
    has_bug_id = False
    for option in options:
      if option in self.MODIFIERS:
        modifiers.add(option)
      elif option in self.PLATFORMS:
        has_any_platform = True
      elif option.startswith('bug'):
        has_bug_id = True
      elif option not in self.BUILD_TYPES:
        self._AddError(lineno, 'Invalid modifier for test: %s' % option,
            test_and_expectations)

    if has_any_platform and not self._platform in options:
      return False

    if not has_bug_id and 'wontfix' not in options:
      # TODO(ojan): Turn this into an AddError call once all the tests have
      # BUG identifiers.
      self._LogNonFatalError(lineno, 'Test lacks BUG modifier.',
          test_and_expectations)

    if 'release' in options or 'debug' in options:
      if self._is_debug_mode and 'debug' not in options:
        return False
      if not self._is_debug_mode and 'release' not in options:
        return False

    if 'wontfix' in options and 'defer' in options:
      self._AddError(lineno, 'Test cannot be both DEFER and WONTFIX.',
          test_and_expectations)

    return True

  def _Read(self, path):
    """For each test in an expectations file, generate the expectations for it.
    """
    lineno = 0
    for line in open(path):
      lineno += 1
      line = StripComments(line)
      if not line: continue

      modifiers = set()
      if line.find(':') is -1:
        test_and_expectations = line
      else:
        parts = line.split(':')
        test_and_expectations = parts[1]
        options = self._GetOptionsList(parts[0])
        if not self._HasValidModifiersForCurrentPlatform(options, lineno,
            test_and_expectations, modifiers):
          continue

      tests_and_expecation_parts = test_and_expectations.split('=')
      if (len(tests_and_expecation_parts) is not 2):
        self._AddError(lineno, 'Missing expectations.', test_and_expectations)
        continue

      test_list_path = tests_and_expecation_parts[0].strip()
      expectations = self._ParseExpectations(tests_and_expecation_parts[1],
          lineno, test_list_path)

      if 'slow' in options and TIMEOUT in expectations:
        self._AddError(lineno, 'A test cannot be both slow and timeout. If the '
            'test times out indefinitely, the it should be listed as timeout.',
            test_and_expectations)

      full_path = os.path.join(path_utils.LayoutDataDir(), test_list_path)
      full_path = os.path.normpath(full_path)
      # WebKit's way of skipping tests is to add a -disabled suffix.
      # So we should consider the path existing if the path or the -disabled
      # version exists.
      if not os.path.exists(full_path) and not \
        os.path.exists(full_path + '-disabled'):
        # Log a non fatal error here since you hit this case any time you
        # update test_expectations.txt without syncing the LayoutTests
        # directory
        self._LogNonFatalError(lineno, 'Path does not exist.', test_list_path)
        continue

      if not self._full_test_list:
        tests = [test_list_path]
      else:
        tests = self._ExpandTests(test_list_path)

      self._AddTests(tests, expectations, test_list_path, lineno, modifiers)

    if len(self._errors) or len(self._non_fatal_errors):
      if self._is_debug_mode:
        build_type = 'DEBUG'
      else:
        build_type = 'RELEASE'
      print "\nFAILURES FOR PLATFORM: %s, BUILD_TYPE: %s" \
          % (self._platform.upper(), build_type)
      for error in self._non_fatal_errors:
        logging.error(error)
      if len(self._errors):
        raise SyntaxError('\n'.join(map(str, self._errors)))

  def _GetOptionsList(self, listString):
    return [part.strip().lower() for part in listString.strip().split(' ')]

  def _ParseExpectations(self, string, lineno, test_list_path):
    result = set()
    for part in self._GetOptionsList(string):
      if not part in self.EXPECTATIONS:
        self._AddError(lineno, 'Unsupported expectation: %s' % part,
            test_list_path)
        continue
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
    
  def _AddTests(self, tests, expectations, test_list_path, lineno, modifiers):
    for test in tests:
      if self._AlreadySeenTest(test, test_list_path, lineno):
        continue

      self._ClearExpectationsForTest(test, test_list_path)
      self._test_to_expectations[test] = expectations

      if len(modifiers) is 0:
        self._AddTest(test, NONE, expectations)
      else:
        for modifier in modifiers:
          self._AddTest(test, self.MODIFIERS[modifier], expectations)

  def _AddTest(self, test, modifier, expectations):
    self._modifier_to_tests[modifier].add(test)
    for expectation in expectations:
      self._expectation_to_tests[expectation].add(test)

  def _ClearExpectationsForTest(self, test, test_list_path):
    """Remove prexisiting expectations for this test.
    This happens if we are seeing a more precise path
    than a previous listing.
    """
    if test in self._test_list_paths:
      self._test_to_expectations.pop(test, '')
      for expectation in self.EXPECTATIONS.itervalues():
        if test in self._expectation_to_tests[expectation]:
          self._expectation_to_tests[expectation].remove(test)
      for modifier in self.MODIFIERS.itervalues():
        if test in self._modifier_to_tests[modifier]:
          self._modifier_to_tests[modifier].remove(test)

    self._test_list_paths[test] = os.path.normpath(test_list_path)

  def _AlreadySeenTest(self, test, test_list_path, lineno):
    """Returns true if we've already seen a more precise path for this test
    than the test_list_path.
    """
    if not test in self._test_list_paths:
      return False

    prev_base_path = self._test_list_paths[test]
    if (prev_base_path == os.path.normpath(test_list_path)):
      self._AddError(lineno, 'Duplicate expectations.', test)
      return True

    # Check if we've already seen a more precise path.
    return prev_base_path.startswith(os.path.normpath(test_list_path))

  def _AddError(self, lineno, msg, path):
    """Reports an error that will prevent running the tests. Does not
    immediately raise an exception because we'd like to aggregate all the
    errors so they can all be printed out."""
    self._errors.append('\nLine:%s %s %s' % (lineno, msg, path))

  def _LogNonFatalError(self, lineno, msg, path):
    """Reports an error that will not prevent running the tests. These are
    still errors, but not bad enough to warrant breaking test running."""
    self._non_fatal_errors.append('Line:%s %s %s' % (lineno, msg, path))
