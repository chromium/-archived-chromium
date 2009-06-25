#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run layout tests using the test_shell.

This is a port of the existing webkit test script run-webkit-tests.

The TestRunner class runs a series of tests (TestType interface) against a set
of test files.  If a test file fails a TestType, it returns a list TestFailure
objects to the TestRunner.  The TestRunner then aggregates the TestFailures to
create a final report.

This script reads several files, if they exist in the test_lists subdirectory
next to this script itself.  Each should contain a list of paths to individual
tests or entire subdirectories of tests, relative to the outermost test
directory.  Entire lines starting with '//' (comments) will be ignored.

For details of the files' contents and purposes, see test_lists/README.
"""

import errno
import glob
import logging
import math
import optparse
import os
import Queue
import random
import shutil
import subprocess
import sys
import time
import traceback

import google.path_utils

from layout_package import compare_failures
from layout_package import test_expectations
from layout_package import http_server
from layout_package import path_utils
from layout_package import platform_utils
from layout_package import test_failures
from layout_package import test_shell_thread
from test_types import fuzzy_image_diff
from test_types import image_diff
from test_types import test_type_base
from test_types import text_diff
from test_types import simplified_text_diff

class TestInfo:
  """Groups information about a test for easy passing of data."""
  def __init__(self, filename, timeout, platform):
    """Generates the URI and stores the filename and timeout for this test.
    Args:
      filename: Full path to the test.
      timeout: Timeout for running the test in TestShell.
      platform: The platform whose test expected results to grab.
      """
    self.filename = filename
    self.uri = path_utils.FilenameToUri(filename)
    self.timeout = timeout
    expected_hash_file = path_utils.ExpectedFilename(filename,
                                                     '.checksum',
                                                     platform)
    try:
      self.image_hash = open(expected_hash_file, "r").read()
    except IOError, e:
      if errno.ENOENT != e.errno:
        raise
      self.image_hash = None


class TestRunner:
  """A class for managing running a series of tests on a series of test
  files."""

  # When collecting test cases, we include any file with these extensions.
  _supported_file_extensions = set(['.html', '.shtml', '.xml', '.xhtml', '.pl',
                                    '.php', '.svg'])
  # When collecting test cases, skip these directories
  _skipped_directories = set(['.svn', '_svn', 'resources'])

  # Top-level directories to shard when running tests.
  _shardable_directories = set(['chrome', 'LayoutTests'])

  HTTP_SUBDIR = os.sep.join(['', 'http', ''])

  # The per-test timeout in milliseconds, if no --time-out-ms option was given
  # to run_webkit_tests. This should correspond to the default timeout in
  # test_shell.exe.
  DEFAULT_TEST_TIMEOUT_MS = 10 * 1000

  def __init__(self, options, paths, platform_new_results_dir):
    """Collect a list of files to test.

    Args:
      options: a dictionary of command line options
      paths: a list of paths to crawl looking for test files
      platform_new_results_dir: name of leaf directory to put rebaselined files
                                in.
    """
    self._options = options
    self._platform_new_results_dir = platform_new_results_dir

    self._http_server = http_server.Lighttpd(options.results_directory)
    # a list of TestType objects
    self._test_types = []

    # a set of test files, and the same tests as a list
    self._test_files = set()
    self._test_files_list = None
    self._file_dir = path_utils.GetAbsolutePath(os.path.dirname(sys.argv[0]))

    if options.lint_test_files:
      # Creating the expecations for each platform/target pair does all the
      # test list parsing and ensures it's correct syntax(e.g. no dupes).
      self._ParseExpectations('win', is_debug_mode=True)
      self._ParseExpectations('win', is_debug_mode=False)
      self._ParseExpectations('mac', is_debug_mode=True)
      self._ParseExpectations('mac', is_debug_mode=False)
      self._ParseExpectations('linux', is_debug_mode=True)
      self._ParseExpectations('linux', is_debug_mode=False)
    else:
      self._GatherTestFiles(paths)
      self._expectations = self._ParseExpectations(
          platform_utils.GetTestListPlatformName().lower(),
          options.target == 'Debug')
      self._PrepareListsAndPrintOutput()

  def __del__(self):
    logging.info("flushing stdout")
    sys.stdout.flush()
    logging.info("flushing stderr")
    sys.stderr.flush()
    logging.info("stopping http server")
    # Stop the http server.
    self._http_server.Stop()

  def _GatherTestFiles(self, paths):
    """Generate a set of test files and place them in self._test_files

    Args:
      paths: a list of command line paths relative to the webkit/tests
             directory.  glob patterns are ok.
    """
    paths_to_walk = set()
    for path in paths:
      # If there's an * in the name, assume it's a glob pattern.
      path = os.path.join(path_utils.LayoutDataDir(), path)
      if path.find('*') > -1:
        filenames = glob.glob(path)
        paths_to_walk.update(filenames)
      else:
        paths_to_walk.add(path)

    # Now walk all the paths passed in on the command line and get filenames
    for path in paths_to_walk:
      if os.path.isfile(path) and self._HasSupportedExtension(path):
        self._test_files.add(os.path.normpath(path))
        continue

      for root, dirs, files in os.walk(path):
        # don't walk skipped directories and sub directories
        if os.path.basename(root) in TestRunner._skipped_directories:
          del dirs[:]
          continue

        for filename in files:
          if self._HasSupportedExtension(filename):
            filename = os.path.join(root, filename)
            filename = os.path.normpath(filename)
            self._test_files.add(filename)

    logging.info('Found: %d tests' % len(self._test_files))

  def _ParseExpectations(self, platform, is_debug_mode):
    """Parse the expectations from the test_list files and return a data
    structure holding them. Throws an error if the test_list files have invalid
    syntax.
    """
    if self._options.lint_test_files:
      test_files = None
    else:
      test_files = self._test_files

    try:
      return test_expectations.TestExpectations(test_files,
                                                self._file_dir,
                                                platform,
                                                is_debug_mode)
    except Exception, err:
      if self._options.lint_test_files:
        print str(err)
      else:
        raise err

  def _PrepareListsAndPrintOutput(self):
    """Create appropriate subsets of test lists and print test counts.

    Create appropriate subsets of self._tests_files in
    self._ignored_failures, self._fixable_failures, and self._fixable_crashes.
    Also remove skipped files from self._test_files, extract a subset of tests
    if desired, and create the sorted self._test_files_list.
    """
    # Remove skipped - both fixable and ignored - files from the
    # top-level list of files to test.
    skipped = set()
    # If there was only one test file, we'll run it even if it was skipped.
    if len(self._test_files) > 1 and not self._options.force:
      skipped = (self._expectations.GetSkipped() |
                 self._expectations.GetWontFixSkipped())
      self._test_files -= skipped

    if self._options.force:
      logging.info('Skipped: 0 tests (--force)')
    else:
      logging.info('Skipped: %d tests' % len(skipped))
    logging.info('Skipped tests do not appear in any of the below numbers\n')

    # Create a sorted list of test files so the subset chunk, if used, contains
    # alphabetically consecutive tests.
    self._test_files_list = list(self._test_files)
    if self._options.randomize_order:
      random.shuffle(self._test_files_list)
    else:
      self._test_files_list.sort(self.TestFilesSort)

    # If the user specifies they just want to run a subset of the tests,
    # just grab a subset of the non-skipped tests.
    if self._options.run_chunk or self._options.run_part:
      chunk_value = self._options.run_chunk or self._options.run_part
      test_files = self._test_files_list
      try:
        (chunk_num, chunk_len) = chunk_value.split(":")
        chunk_num = int(chunk_num)
        assert(chunk_num >= 0)
        test_size = int(chunk_len)
        assert(test_size > 0)
      except:
        logging.critical("invalid chunk '%s'" % chunk_value)
        sys.exit(1)

      # Get the number of tests
      num_tests = len(test_files)

      # Get the start offset of the slice.
      if self._options.run_chunk:
        chunk_len = test_size
        # In this case chunk_num can be really large. We need to make the
        # slave fit in the current number of tests.
        slice_start = (chunk_num * chunk_len) % num_tests
      else:
        # Validate the data.
        assert(test_size <= num_tests)
        assert(chunk_num <= test_size)

        # To count the chunk_len, and make sure we don't skip some tests, we
        # round to the next value that fits exacly all the parts.
        rounded_tests = num_tests
        if rounded_tests % test_size != 0:
          rounded_tests = num_tests + test_size - (num_tests % test_size)

        chunk_len = rounded_tests / test_size
        slice_start = chunk_len * (chunk_num - 1)
        # It does not mind if we go over test_size.

      # Get the end offset of the slice.
      slice_end = min(num_tests, slice_start + chunk_len)

      files = test_files[slice_start:slice_end]
      logging.info('Run: %d tests (chunk slice [%d:%d] of %d)' % (
          (slice_end - slice_start), slice_start, slice_end, num_tests))

      # If we reached the end and we don't have enough tests, we run some
      # from the beginning.
      if self._options.run_chunk and (slice_end - slice_start < chunk_len):
        extra = 1 + chunk_len - (slice_end - slice_start)
        logging.info('   last chunk is partial, appending [0:%d]' % extra)
        files.extend(test_files[0:extra])
      self._test_files_list = files
      self._test_files = set(files)

      # update expectations so that the stats are calculated correctly
      self._expectations = self._ParseExpectations(
          platform_utils.GetTestListPlatformName().lower(),
          options.target == 'Debug')
    else:
      logging.info('Run: %d tests' % len(self._test_files))

    logging.info('Deferred: %d tests' % len(self._expectations.GetDeferred()))
    logging.info('Expected passes: %d tests' %
                 len(self._test_files -
                     self._expectations.GetFixable() -
                     self._expectations.GetWontFix()))
    logging.info(('Expected failures: %d fixable, %d ignored '
                  'and %d deferred tests') %
                 (len(self._expectations.GetFixableFailures()),
                  len(self._expectations.GetWontFixFailures()),
                  len(self._expectations.GetDeferredFailures())))
    logging.info(('Expected timeouts: %d fixable, %d ignored '
                  'and %d deferred tests') %
                 (len(self._expectations.GetFixableTimeouts()),
                  len(self._expectations.GetWontFixTimeouts()),
                  len(self._expectations.GetDeferredTimeouts())))
    logging.info('Expected crashes: %d fixable tests' %
                 len(self._expectations.GetFixableCrashes()))

  def _HasSupportedExtension(self, filename):
    """Return true if filename is one of the file extensions we want to run a
    test on."""
    extension = os.path.splitext(filename)[1]
    return extension in TestRunner._supported_file_extensions

  def AddTestType(self, test_type):
    """Add a TestType to the TestRunner."""
    self._test_types.append(test_type)

  # We sort the tests so that tests using the http server will run first.  We
  # are seeing some flakiness, maybe related to apache getting swapped out,
  # slow, or stuck after not serving requests for a while.
  def TestFilesSort(self, x, y):
    """Sort with http tests always first."""
    x_is_http = x.find(self.HTTP_SUBDIR) >= 0
    y_is_http = y.find(self.HTTP_SUBDIR) >= 0
    if x_is_http != y_is_http:
      return cmp(y_is_http, x_is_http)
    return cmp(x, y)

  def _GetDirForTestFile(self, test_file):
    """Returns the highest-level directory by which to shard the given test
    file."""
    # TODO(ojan): See if we can grab the lowest level directory. That will
    # provide better parallelization. We should at least be able to do so
    # for some directories (e.g. LayoutTests/dom).
    index = test_file.rfind(os.sep + 'LayoutTests' + os.sep)
    if index is -1:
      index = test_file.rfind(os.sep + 'chrome' + os.sep)

    test_file = test_file[index + 1:]
    test_file_parts = test_file.split(os.sep, 1)
    directory = test_file_parts[0]
    test_file = test_file_parts[1]

    return_value = directory
    while directory in TestRunner._shardable_directories:
      test_file_parts = test_file.split(os.sep, 1)
      directory = test_file_parts[0]
      return_value = os.path.join(return_value, directory)
      test_file = test_file_parts[1]

    return return_value

  def _GetTestFileQueue(self, test_files):
    """Create the thread safe queue of lists of (test filenames, test URIs)
    tuples. Each TestShellThread pulls a list from this queue and runs those
    tests in order before grabbing the next available list.

    Shard the lists by directory. This helps ensure that tests that depend
    on each other (aka bad tests!) continue to run together as most
    cross-tests dependencies tend to occur within the same directory.

    Return:
      The Queue of lists of TestInfo objects.
    """
    tests_by_dir = {}
    for test_file in test_files:
      directory = self._GetDirForTestFile(test_file)
      if directory not in tests_by_dir:
        tests_by_dir[directory] = []

      if self._expectations.HasModifier(test_file, test_expectations.SLOW):
        timeout = 10 * int(options.time_out_ms)
      else:
        timeout = self._options.time_out_ms

      tests_by_dir[directory].append(TestInfo(test_file, timeout,
          self._options.platform))

    # Sort by the number of tests in the dir so that the ones with the most
    # tests get run first in order to maximize parallelization. Number of tests
    # is a good enough, but not perfect, approximation of how long that set of
    # tests will take to run. We can't just use a PriorityQueue until we move
    # to Python 2.6.
    test_lists = []
    http_tests = None
    for directory in tests_by_dir:
      test_list = tests_by_dir[directory]
      # Keep the tests in alphabetical order.
      # TODO: Remove once tests are fixed so they can be run in any order.
      test_list.reverse()
      test_list_tuple = (directory, test_list)
      if directory == 'LayoutTests' + os.sep + 'http':
        http_tests = test_list_tuple
      else:
        test_lists.append(test_list_tuple)
    test_lists.sort(lambda a, b: cmp(len(b[1]), len(a[1])))

    # Put the http tests first. There are only a couple hundred of them, but
    # each http test takes a very long time to run, so sorting by the number
    # of tests doesn't accurately capture how long they take to run.
    if http_tests:
      test_lists.insert(0, http_tests)

    filename_queue = Queue.Queue()
    for item in test_lists:
      filename_queue.put(item)
    return filename_queue

  def _GetTestShellArgs(self, index):
    """Returns the tuple of arguments for tests and for test_shell."""
    shell_args = []
    test_args = test_type_base.TestArguments()
    if not self._options.no_pixel_tests:
      png_path = os.path.join(self._options.results_directory,
                              "png_result%s.png" % index)
      shell_args.append("--pixel-tests=" + png_path)
      test_args.png_path = png_path

    test_args.new_baseline = self._options.new_baseline
    test_args.show_sources = self._options.sources

    if self._options.startup_dialog:
      shell_args.append('--testshell-startup-dialog')

    if self._options.gp_fault_error_box:
      shell_args.append('--gp-fault-error-box')

    return (test_args, shell_args)

  def _InstantiateTestShellThreads(self, test_shell_binary):
    """Instantitates and starts the TestShellThread(s).

    Return:
      The list of threads.
    """
    test_shell_command = [test_shell_binary]

    if self._options.wrapper:
      # This split() isn't really what we want -- it incorrectly will
      # split quoted strings within the wrapper argument -- but in
      # practice it shouldn't come up and the --help output warns
      # about it anyway.
      test_shell_command = self._options.wrapper.split() + test_shell_command

    test_files = self._test_files_list
    filename_queue = self._GetTestFileQueue(test_files)

    # If we have http tests, the first one will be an http test.
    if test_files and test_files[0].find(self.HTTP_SUBDIR) >= 0:
      self._http_server.Start()

    # Instantiate TestShellThreads and start them.
    threads = []
    for i in xrange(int(self._options.num_test_shells)):
      # Create separate TestTypes instances for each thread.
      test_types = []
      for t in self._test_types:
        test_types.append(t(self._options.platform,
                            self._options.results_directory,
                            self._platform_new_results_dir))

      test_args, shell_args = self._GetTestShellArgs(i)
      thread = test_shell_thread.TestShellThread(filename_queue,
                                                 test_shell_command,
                                                 test_types,
                                                 test_args,
                                                 shell_args,
                                                 self._options)
      thread.start()
      threads.append(thread)

    return threads

  def _StopLayoutTestHelper(self, proc):
    """Stop the layout test helper and closes it down."""
    if proc:
      logging.info("Stopping layout test helper")
      proc.stdin.write("x\n")
      proc.stdin.close()
      proc.wait()

  def Run(self):
    """Run all our tests on all our test files.

    For each test file, we run each test type.  If there are any failures, we
    collect them for reporting.

    Return:
      We return nonzero if there are regressions compared to the last run.
    """
    if not self._test_files:
      return 0
    start_time = time.time()
    test_shell_binary = path_utils.TestShellBinaryPath(self._options.target)

    # Check that the system dependencies (themes, fonts, ...) are correct.
    if not self._options.nocheck_sys_deps:
      proc = subprocess.Popen([test_shell_binary,
                              "--check-layout-test-sys-deps"])
      if proc.wait() != 0:
        logging.info("Aborting because system dependencies check failed.\n"
                     "To override, invoke with --nocheck-sys-deps")
        sys.exit(1)

    logging.info("Starting tests")

    # Create the output directory if it doesn't already exist.
    google.path_utils.MaybeMakeDirectory(self._options.results_directory)

    # Start up any helper needed
    layout_test_helper_proc = None
    if sys.platform in ('darwin'):
      # Mac uses a helper for manging the color sync profile for pixel tests.
      if not options.no_pixel_tests:
        helper_path = \
            path_utils.LayoutTestHelperBinaryPath(self._options.target)
        logging.info("Starting layout helper %s" % helper_path)
        layout_test_helper_proc = subprocess.Popen([helper_path],
                                                   stdin=subprocess.PIPE,
                                                   stdout=subprocess.PIPE,
                                                   stderr=None)
        is_ready = layout_test_helper_proc.stdout.readline()
        if is_ready != 'ready\n':
          logging.error("layout_test_helper failed to be ready")

    threads = self._InstantiateTestShellThreads(test_shell_binary)

    # Wait for the threads to finish and collect test failures.
    failures = {}
    test_timings = {}
    individual_test_timings = []
    try:
      for thread in threads:
        while thread.isAlive():
          # Let it timeout occasionally so it can notice a KeyboardInterrupt
          # Actually, the timeout doesn't really matter: apparently it
          # suffices to not use an indefinite blocking join for it to
          # be interruptible by KeyboardInterrupt.
          thread.join(1.0)
        failures.update(thread.GetFailures())
        test_timings.update(thread.GetDirectoryTimingStats())
        individual_test_timings.extend(thread.GetIndividualTestStats())
    except KeyboardInterrupt:
      for thread in threads:
        thread.Cancel()
      self._StopLayoutTestHelper(layout_test_helper_proc)
      raise
    self._StopLayoutTestHelper(layout_test_helper_proc)
    for thread in threads:
      # Check whether a TestShellThread died before normal completion.
      exception_info = thread.GetExceptionInfo()
      if exception_info is not None:
        # Re-raise the thread's exception here to make it clear that
        # testing was aborted. Otherwise, the tests that did not run
        # would be assumed to have passed.
        raise exception_info[0], exception_info[1], exception_info[2]

    print
    end_time = time.time()
    logging.info("%f total testing time" % (end_time - start_time))

    print
    self._PrintTimingStatistics(test_timings, individual_test_timings, failures)

    print "-" * 78

    # Tests are done running. Compare failures with expected failures.
    regressions = self._CompareFailures(failures)

    print "-" * 78

    # Write summaries to stdout.
    self._PrintResults(failures, sys.stdout)

    # Write the same data to a log file.
    out_filename = os.path.join(self._options.results_directory, "score.txt")
    output_file = open(out_filename, "w")
    self._PrintResults(failures, output_file)
    output_file.close()

    # Write the summary to disk (results.html) and maybe open the test_shell
    # to this file.
    wrote_results = self._WriteResultsHtmlFile(failures, regressions)
    if not self._options.noshow_results and wrote_results:
      self._ShowResultsHtmlFile()

    sys.stdout.flush()
    sys.stderr.flush()
    return len(regressions)

  def _PrintTimingStatistics(self, directory_test_timings,
      individual_test_timings, failures):
    self._PrintAggregateTestStatistics(individual_test_timings)
    self._PrintIndividualTestTimes(individual_test_timings, failures)
    self._PrintDirectoryTimings(directory_test_timings)

  def _PrintAggregateTestStatistics(self, individual_test_timings):
    """Prints aggregate statistics (e.g. median, mean, etc.) for all tests.
    Args:
      individual_test_timings: List of test_shell_thread.TestStats for all
          tests.
    """
    test_types = individual_test_timings[0].time_for_diffs.keys()
    times_for_test_shell = []
    times_for_diff_processing = []
    times_per_test_type = {}
    for test_type in test_types:
      times_per_test_type[test_type] = []

    for test_stats in individual_test_timings:
      times_for_test_shell.append(test_stats.test_run_time)
      times_for_diff_processing.append(test_stats.total_time_for_all_diffs)
      time_for_diffs = test_stats.time_for_diffs
      for test_type in test_types:
        times_per_test_type[test_type].append(time_for_diffs[test_type])

    self._PrintStatisticsForTestTimings(
        "PER TEST TIME IN TESTSHELL (seconds):",
        times_for_test_shell)
    self._PrintStatisticsForTestTimings(
        "PER TEST DIFF PROCESSING TIMES (seconds):",
        times_for_diff_processing)
    for test_type in test_types:
      self._PrintStatisticsForTestTimings(
          "PER TEST TIMES BY TEST TYPE: %s" % test_type,
          times_per_test_type[test_type])

  def _PrintIndividualTestTimes(self, individual_test_timings, failures):
    """Prints the run times for slow, timeout and crash tests.
    Args:
      individual_test_timings: List of test_shell_thread.TestStats for all
          tests.
      failures: Dictionary mapping test filenames to list of test_failures.
    """
    # Reverse-sort by the time spent in test_shell.
    individual_test_timings.sort(lambda a, b:
        cmp(b.test_run_time, a.test_run_time))

    num_printed = 0
    slow_tests = []
    timeout_or_crash_tests = []
    unexpected_slow_tests = []
    for test_tuple in individual_test_timings:
      filename = test_tuple.filename
      is_timeout_crash_or_slow = False
      if self._expectations.HasModifier(filename, test_expectations.SLOW):
        is_timeout_crash_or_slow = True
        slow_tests.append(test_tuple)

      if filename in failures:
        for failure in failures[filename]:
          if (failure.__class__ == test_failures.FailureTimeout or
              failure.__class__ == test_failures.FailureCrash):
            is_timeout_crash_or_slow = True
            timeout_or_crash_tests.append(test_tuple)
            break

      if (not is_timeout_crash_or_slow and
          num_printed < self._options.num_slow_tests_to_log):
        num_printed = num_printed + 1
        unexpected_slow_tests.append(test_tuple)

    print
    self._PrintTestListTiming("%s slowest tests that are not marked as SLOW "
        "and did not timeout/crash:" % self._options.num_slow_tests_to_log,
        unexpected_slow_tests)
    print
    self._PrintTestListTiming("Tests marked as SLOW:", slow_tests)
    print
    self._PrintTestListTiming("Tests that timed out or crashed:",
        timeout_or_crash_tests)
    print

  def _PrintTestListTiming(self, title, test_list):
    logging.debug(title)
    for test_tuple in test_list:
      filename = test_tuple.filename[len(path_utils.LayoutDataDir()) + 1:]
      filename = filename.replace('\\', '/')
      test_run_time = round(test_tuple.test_run_time, 1)
      logging.debug("%s took %s seconds" % (filename, test_run_time))

  def _PrintDirectoryTimings(self, directory_test_timings):
    timings = []
    for directory in directory_test_timings:
      num_tests, time_for_directory = directory_test_timings[directory]
      timings.append((round(time_for_directory, 1), directory, num_tests))
    timings.sort()

    logging.debug("Time to process each subdirectory:")
    for timing in timings:
      logging.debug("%s took %s seconds to run %s tests." % \
                    (timing[1], timing[0], timing[2]))

  def _PrintStatisticsForTestTimings(self, title, timings):
    """Prints the median, mean and standard deviation of the values in timings.
    Args:
      title: Title for these timings.
      timings: A list of floats representing times.
    """
    logging.debug(title)
    timings.sort()

    num_tests = len(timings)
    percentile90 = timings[int(.9 * num_tests)]
    percentile99 = timings[int(.99 * num_tests)]

    if num_tests % 2 == 1:
      median = timings[((num_tests - 1) / 2) - 1]
    else:
      lower = timings[num_tests / 2 - 1]
      upper = timings[num_tests / 2]
      median = (float(lower + upper)) / 2

    mean = sum(timings) / num_tests

    for time in timings:
      sum_of_deviations = math.pow(time - mean, 2)

    std_deviation = math.sqrt(sum_of_deviations / num_tests)
    logging.debug(("Median: %s, Mean: %s, 90th percentile: %s, "
        "99th percentile: %s, Standard deviation: %s\n" % (
        median, mean, percentile90, percentile99, std_deviation)))

  def _PrintResults(self, failures, output):
    """Print a short summary to stdout about how many tests passed.

    Args:
      failures is a dictionary mapping the test filename to a list of
      TestFailure objects if the test failed

      output is the file descriptor to write the results to. For example,
      sys.stdout.
    """

    failure_counts = {}
    deferred_counts = {}
    fixable_counts = {}
    non_ignored_counts = {}
    fixable_failures = set()
    deferred_failures = set()
    non_ignored_failures = set()

    # Aggregate failures in a dictionary (TestFailure -> frequency),
    # with known (fixable and ignored) failures separated out.
    def AddFailure(dictionary, key):
      if key in dictionary:
        dictionary[key] += 1
      else:
        dictionary[key] = 1

    for test, failures in failures.iteritems():
      for failure in failures:
        AddFailure(failure_counts, failure.__class__)
        if self._expectations.IsDeferred(test):
          AddFailure(deferred_counts, failure.__class__)
          deferred_failures.add(test)
        else:
          if self._expectations.IsFixable(test):
            AddFailure(fixable_counts, failure.__class__)
            fixable_failures.add(test)
          if not self._expectations.IsIgnored(test):
            AddFailure(non_ignored_counts, failure.__class__)
            non_ignored_failures.add(test)

    # Print breakdown of tests we need to fix and want to pass.
    # Include skipped fixable tests in the statistics.
    skipped = (self._expectations.GetSkipped() -
        self._expectations.GetDeferredSkipped())

    self._PrintResultSummary("=> Tests to be fixed for the current release",
                             self._expectations.GetFixable(),
                             fixable_failures,
                             fixable_counts,
                             skipped,
                             output)

    self._PrintResultSummary("=> Tests we want to pass for the current release",
                             (self._test_files -
                              self._expectations.GetWontFix() -
                              self._expectations.GetDeferred()),
                             non_ignored_failures,
                             non_ignored_counts,
                             skipped,
                             output)

    self._PrintResultSummary("=> Tests to be fixed for a future release",
                             self._expectations.GetDeferred(),
                             deferred_failures,
                             deferred_counts,
                             self._expectations.GetDeferredSkipped(),
                             output)

    # Print breakdown of all tests including all skipped tests.
    skipped |= self._expectations.GetWontFixSkipped()
    self._PrintResultSummary("=> All tests",
                             self._test_files,
                             failures,
                             failure_counts,
                             skipped,
                             output)
    print

  def _PrintResultSummary(self, heading, all, failed, failure_counts, skipped,
                          output):
    """Print a summary block of results for a particular category of test.

    Args:
      heading: text to print before the block, followed by the total count
      all: list of all tests in this category
      failed: list of failing tests in this category
      failure_counts: dictionary of (TestFailure -> frequency)
      output: file descriptor to write the results to
    """
    total = len(all | skipped)
    output.write("\n%s (%d):\n" % (heading, total))
    skip_count = len(skipped)
    pass_count = total - skip_count - len(failed)
    self._PrintResultLine(pass_count, total, "Passed", output)
    self._PrintResultLine(skip_count, total, "Skipped", output)
    # Sort the failure counts and print them one by one.
    sorted_keys = sorted(failure_counts.keys(),
                         key=test_failures.FailureSort.SortOrder)
    for failure in sorted_keys:
      self._PrintResultLine(failure_counts[failure], total, failure.Message(),
                            output)

  def _PrintResultLine(self, count, total, message, output):
    if count == 0: return
    output.write(
        ("%(count)d test case%(plural)s (%(percent).1f%%) %(message)s\n" %
            { 'count'   : count,
              'plural'  : ('s', '')[count == 1],
              'percent' : float(count) * 100 / total,
              'message' : message }))

  def _CompareFailures(self, failures):
    """Determine how the test failures from this test run differ from the
    previous test run and print results to stdout and a file.

    Args:
      failures is a dictionary mapping the test filename to a list of
      TestFailure objects if the test failed

    Return:
      A set of regressions (unexpected failures, hangs, or crashes)
    """
    cf = compare_failures.CompareFailures(self._test_files,
                                          failures,
                                          self._expectations)

    if not self._options.nocompare_failures:
      cf.PrintRegressions(sys.stdout)

      out_filename = os.path.join(self._options.results_directory,
                                  "regressions.txt")
      output_file = open(out_filename, "w")
      cf.PrintRegressions(output_file)
      output_file.close()

    return cf.GetRegressions()

  def _WriteResultsHtmlFile(self, failures, regressions):
    """Write results.html which is a summary of tests that failed.

    Args:
      failures: a dictionary mapping the test filename to a list of
          TestFailure objects if the test failed
      regressions: a set of test filenames that regressed

    Returns:
      True if any results were written (since expected failures may be omitted)
    """
    # test failures
    if self._options.full_results_html:
      test_files = failures.keys()
    else:
      test_files = list(regressions)
    if not len(test_files):
      return False

    out_filename = os.path.join(self._options.results_directory,
                                "results.html")
    out_file = open(out_filename, 'w')
    # header
    if self._options.full_results_html:
      h2 = "Test Failures"
    else:
      h2 = "Unexpected Test Failures"
    out_file.write("<html><head><title>Layout Test Results (%(time)s)</title>"
                   "</head><body><h2>%(h2)s (%(time)s)</h2>\n"
                   % {'h2': h2, 'time': time.asctime()})

    test_files.sort()
    for test_file in test_files:
      if test_file in failures: test_failures = failures[test_file]
      else: test_failures = []  # unexpected passes
      out_file.write("<p><a href='%s'>%s</a><br />\n"
                     % (path_utils.FilenameToUri(test_file),
                        path_utils.RelativeTestFilename(test_file)))
      for failure in test_failures:
        out_file.write("&nbsp;&nbsp;%s<br/>"
                       % failure.ResultHtmlOutput(
                         path_utils.RelativeTestFilename(test_file)))
      out_file.write("</p>\n")

    # footer
    out_file.write("</body></html>\n")
    return True

  def _ShowResultsHtmlFile(self):
    """Launches the test shell open to the results.html page."""
    results_filename = os.path.join(self._options.results_directory,
                                    "results.html")
    subprocess.Popen([path_utils.TestShellBinaryPath(self._options.target),
                      path_utils.FilenameToUri(results_filename)])


def ReadTestFiles(files):
  tests = []
  for file in files:
    for line in open(file):
      line = test_expectations.StripComments(line)
      if line: tests.append(line)
  return tests

def main(options, args):
  """Run the tests.  Will call sys.exit when complete.

  Args:
    options: a dictionary of command line options
    args: a list of sub directories or files to test
  """

  if options.sources:
    options.verbose = True

  # Set up our logging format.
  log_level = logging.INFO
  if options.verbose:
    log_level = logging.DEBUG
  logging.basicConfig(level=log_level,
                      format='%(asctime)s %(filename)s:%(lineno)-3d'
                             ' %(levelname)s %(message)s',
                      datefmt='%y%m%d %H:%M:%S')

  if not options.target:
    if options.debug:
      options.target = "Debug"
    else:
      options.target = "Release"

  if options.results_directory.startswith("/"):
    # Assume it's an absolute path and normalize
    options.results_directory = path_utils.GetAbsolutePath(
        options.results_directory)
  else:
    # If it's a relative path, make the output directory relative to Debug or
    # Release.
    basedir = path_utils.WebKitRoot()
    basedir = os.path.join(basedir, options.target)

    options.results_directory = path_utils.GetAbsolutePath(
        os.path.join(basedir, options.results_directory))

  if options.platform is None:
    options.platform = path_utils.PlatformDir()
    platform_new_results_dir = path_utils.PlatformNewResultsDir()
  else:
    # If the user specified a platform on the command line then use
    # that as the name of the output directory for rebaselined files.
    platform_new_results_dir = options.platform

  if not options.num_test_shells:
    cpus = 1
    if sys.platform in ('win32', 'cygwin'):
      cpus = int(os.environ.get('NUMBER_OF_PROCESSORS', 1))
    elif (hasattr(os, "sysconf") and
          os.sysconf_names.has_key("SC_NPROCESSORS_ONLN")):
      # Linux & Unix:
      ncpus = os.sysconf("SC_NPROCESSORS_ONLN")
      if isinstance(ncpus, int) and ncpus > 0:
        cpus = ncpus
    elif sys.platform in ('darwin'): # OSX:
      cpus = int(os.popen2("sysctl -n hw.ncpu")[1].read())

    # TODO(ojan): Use cpus+1 once we flesh out the flakiness.
    options.num_test_shells = cpus

  logging.info("Running %s test_shells in parallel" % options.num_test_shells)

  if not options.time_out_ms:
    if options.num_test_shells > 1:
      options.time_out_ms = str(2 * TestRunner.DEFAULT_TEST_TIMEOUT_MS)
    else:
      options.time_out_ms = str(TestRunner.DEFAULT_TEST_TIMEOUT_MS)

  # Include all tests if none are specified.
  paths = args
  if not paths:
    paths = []
  if options.test_list:
    paths += ReadTestFiles(options.test_list)
  if not paths:
    paths = ['.']

  test_runner = TestRunner(options, paths, platform_new_results_dir)

  if options.lint_test_files:
    # Just creating the TestRunner checks the syntax of the test lists.
    print ("If there are no fail messages, errors or exceptions, then the "
        "lint succeeded.")
    return

  try:
    test_shell_binary_path = path_utils.TestShellBinaryPath(options.target)
  except path_utils.PathNotFound:
    print "\nERROR: test_shell is not found. Be sure that you have built it"
    print "and that you are using the correct build. This script will run the"
    print "Release one by default. Use --debug to use the Debug build.\n"
    sys.exit(1)

  logging.info("Using platform '%s'" % options.platform)
  logging.info("Placing test results in %s" % options.results_directory)
  if options.new_baseline:
    logging.info("Placing new baselines in %s" %
                 os.path.join(path_utils.PlatformResultsEnclosingDir(options.platform),
                              platform_new_results_dir))
  logging.info("Using %s build at %s" %
               (options.target, test_shell_binary_path))
  if not options.no_pixel_tests:
    logging.info("Running pixel tests")

  if 'cygwin' == sys.platform:
    logging.warn("#" * 40)
    logging.warn("# UNEXPECTED PYTHON VERSION")
    logging.warn("# This script should be run using the version of python")
    logging.warn("# in third_party/python_24/")
    logging.warn("#" * 40)
    sys.exit(1)

  # Delete the disk cache if any to ensure a clean test run.
  cachedir = os.path.split(test_shell_binary_path)[0]
  cachedir = os.path.join(cachedir, "cache")
  if os.path.exists(cachedir):
    shutil.rmtree(cachedir)

  test_runner.AddTestType(text_diff.TestTextDiff)
  test_runner.AddTestType(simplified_text_diff.SimplifiedTextDiff)
  if not options.no_pixel_tests:
    test_runner.AddTestType(image_diff.ImageDiff)
    if options.fuzzy_pixel_tests:
      test_runner.AddTestType(fuzzy_image_diff.FuzzyImageDiff)
  has_new_failures = test_runner.Run()
  logging.info("Exit status: %d" % has_new_failures)
  sys.exit(has_new_failures)

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option("", "--no-pixel-tests", action="store_true",
                           default=False,
                           help="disable pixel-to-pixel PNG comparisons")
  option_parser.add_option("", "--fuzzy-pixel-tests", action="store_true",
                           default=False,
                           help="Also use fuzzy matching to compare pixel test"
                                " outputs.")
  option_parser.add_option("", "--results-directory",
                           default="layout-test-results",
                           help="Output results directory source dir,"
                                " relative to Debug or Release")
  option_parser.add_option("", "--new-baseline", action="store_true",
                           default=False,
                           help="save all generated results as new baselines "
                                "into the platform directory, overwriting "
                                "whatever's already there.")
  option_parser.add_option("", "--noshow-results", action="store_true",
                           default=False, help="don't launch the test_shell"
                           " with results after the tests are done")
  option_parser.add_option("", "--full-results-html", action="store_true",
                           default=False, help="show all failures in "
                           "results.html, rather than only regressions")
  option_parser.add_option("", "--lint-test-files", action="store_true",
                           default=False, help="Makes sure the test files "
                           "parse for all configurations. Does not run any "
                           "tests.")
  option_parser.add_option("", "--force", action="store_true",
                           default=False,
                           help="Run all tests, even those marked SKIP in the "
                                "test list")
  option_parser.add_option("", "--nocompare-failures", action="store_true",
                           default=False,
                           help="Disable comparison to the last test run. "
                                "When enabled, show stats on how many tests "
                                "newly pass or fail.")
  option_parser.add_option("", "--num-test-shells",
                           help="Number of testshells to run in parallel.")
  option_parser.add_option("", "--time-out-ms", default=None,
                           help="Set the timeout for each test")
  option_parser.add_option("", "--run-singly", action="store_true",
                           default=False,
                           help="run a separate test_shell for each test")
  option_parser.add_option("", "--debug", action="store_true", default=False,
                           help="use the debug binary instead of the release "
                                "binary")
  option_parser.add_option("", "--num-slow-tests-to-log", default=50,
                           help="Number of slow tests whose timings to print.")
  option_parser.add_option("", "--platform",
                           help="Override the platform for expected results")
  option_parser.add_option("", "--target", default="",
                           help="Set the build target configuration (overrides"
                                 " --debug)")
  # TODO(pamg): Support multiple levels of verbosity, and remove --sources.
  option_parser.add_option("-v", "--verbose", action="store_true",
                           default=False, help="include debug-level logging")
  option_parser.add_option("", "--sources", action="store_true",
                           help="show expected result file path for each test "
                                "(implies --verbose)")
  option_parser.add_option("", "--startup-dialog", action="store_true",
                           default=False,
                           help="create a dialog on test_shell.exe startup")
  option_parser.add_option("", "--gp-fault-error-box", action="store_true",
                           default=False,
                           help="enable Windows GP fault error box")
  option_parser.add_option("", "--wrapper",
                           help="wrapper command to insert before invocations "
                                "of test_shell; option is split on whitespace "
                                "before running.  (example: "
                                "--wrapper='valgrind --smc-check=all')")
  option_parser.add_option("", "--test-list", action="append",
                           help="read list of tests to run from file",
                           metavar="FILE")
  option_parser.add_option("", "--nocheck-sys-deps", action="store_true",
                           default=False,
                           help="Don't check the system dependencies (themes)")
  option_parser.add_option("", "--randomize-order", action="store_true",
                           default=False,
                           help=("Run tests in random order (useful for "
                                 "tracking down corruption)"))
  option_parser.add_option("", "--run-chunk",
                           default=None,
                           help=("Run a specified chunk (n:l), the nth of len "
                                 "l, of the layout tests"))
  option_parser.add_option("", "--run-part",
                           default=None,
                           help=("Run a specified part (n:m), the nth of m"
                                 " parts, of the layout tests"))
  option_parser.add_option("", "--batch-size",
                           default=None,
                           help=("Run a the tests in batches (n), after every "
                                 "n tests, the test shell is relaunched."))
  options, args = option_parser.parse_args()
  main(options, args)
