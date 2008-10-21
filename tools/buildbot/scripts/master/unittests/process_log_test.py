#!/usr/bin/python
# Copyright (c) 2007-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Source file for log processor testcases.

These tests should be run from the directory in which the script lives, so it
can find its data/ directory.

"""

import os
import shutil
import stat
import unittest

import pmock
import simplejson

import chromium_step
import chromium_utils
from log_parser import process_log

class GoogleLoggingStepTest(unittest.TestCase):
  """ Logging testcases superclass

  The class provides some operations common for testcases.
  """
  def setUp(self):
    self._RemoveOutputDir()
    self._revision = 12345
    self._report_link = 'http://localhost/~user/report.html'
    self._output_dir = 'output_dir'

  def tearDown(self):
    if os.path.exists(self._output_dir):
      directoryListing = os.listdir(self._output_dir)
      for filename in directoryListing:
        file_stats = os.stat(os.path.join(self._output_dir, filename))
        self._assertReadable(file_stats)

  def _assertReadable(self, file_stats):
    mode = file_stats[stat.ST_MODE]
    self.assertEqual(4, mode & stat.S_IROTH)

  def _ConstructStep(self, log_processor_class, logfile):
    """ Common approach to construct chromium_step.ProcessLogTestStep
    type instance with LogFile instance set.
    Args:
      log_processor_class: type/class of type chromium_step.ProcessLogTestStep
        that is going to be constructed. E.g. PagecyclerTestStep
      logfile: filename with setup process log output.
    """
    log_processor_class = chromium_utils.InitializePartiallyWithArguments(
        log_processor_class, report_link=self._report_link,
        output_dir=self._output_dir)
    step = self._CreateStep(log_processor_class)
    log_file = self._LogFile(
        'stdio', open(os.path.join('data', logfile)).read())
    self._SetupBuild(step, self._revision, log_file)
    return step

  def _CreateStep(self, log_processor_class):
    """ Creates the appropriate step for this test case. This is in its
    own function so it can be overridden by subclasses as needed.
    Args:
      log_processor_class: type/class of type chromium_step.ProcessLogTestStep
        that is going to be constructed. E.g. PagecyclerTestStep
    """
    return chromium_step.ProcessLogShellStep(log_processor_class)

  def _SetupBuild(self, step, revision, log_file):
    build_mock = pmock.Mock()
    build_mock.stubs().getProperty(
        pmock.eq('got_revision')).will(pmock.return_value(self._revision))
    build_mock.expects(
        pmock.once()).getLogs().will(pmock.return_value([log_file]))
    step.step_status = build_mock
    step.build = build_mock

  def _LogFile(self, name, content):
    log_file_mock = pmock.Mock()
    log_file_mock.stubs().getName().will(pmock.return_value(name))
    log_file_mock.stubs().getText().will(pmock.return_value(content))
    return log_file_mock

  def _JoinWithSpaces(self, array):
    return ' '.join([str(x) for x in array])

  def _JoinWithSpacesAndNewLine(self, array):
    return '%s\n' % self._JoinWithSpaces(array)

  def _RemoveOutputDir(self):
    if os.path.exists('output_dir'):
      shutil.rmtree('output_dir')


class PagecyclerTestStepTest(GoogleLoggingStepTest):

  def testReportLink(self):
    report_link='http://localhost/~user/report.html'
    log_processor = process_log.PageCyclerLogProcessor(
        report_link=report_link, output_dir='output_dir')
    self.assertEqual(report_link, log_processor.ReportLink())

  def testVmPeakBrowser(self):
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/summary-vm-peak-browser.dat'))
    actual = open('output_dir/summary-vm-peak-browser.dat').readline()
    self.assertEqual('12345 8151040 4468736\n', actual)

  def testVmPeakRenderer(self):
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/summary-vm-peak-renderer.dat'))
    actual = open('output_dir/summary-vm-peak-renderer.dat').readline()
    self.assertEqual('12345 64729088 52609024\n', actual)

  def testSummaryIoOpBrowser(self):
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log_io')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/summary-io-op-browser.dat'))
    read_op = 12743
    write_op = 8823
    other_op = 26638
    total_op = read_op + write_op + other_op

    read_op_reference = 4594
    write_op_reference = 5834
    other_op_reference = 24627
    total_op_reference = (read_op_reference + write_op_reference +
                          other_op_reference)
    actual = open('output_dir/summary-io-op-browser.dat').readline()
    self.assertEqual('12345 %s %s\n' % (total_op, total_op_reference), actual)

  def testSummaryIoByteBrowser(self):
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log_io')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/summary-io-byte-browser.dat'))
    read_byte = 7654
    write_byte = 6231
    other_byte = 584
    total_byte = read_byte + write_byte + other_byte

    read_byte_reference = 7237
    write_byte_reference = 8891
    other_byte_reference = 515
    total_byte_reference = (read_byte_reference +
                            write_byte_reference + other_byte_reference)
    actual = open('output_dir/summary-io-byte-browser.dat').readline()
    self.assertEqual('12345 %s %s\n'
                     % (total_byte, total_byte_reference), actual)

  def testDetailIoBrowser(self):
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log_io')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/detail-io-browser.dat'))
    actual = open('output_dir/detail-io-browser.dat').readline()
    expected = ('12345 12743 8823 26638 7654 6231 584 '
                '4594 5834 24627 7237 8891 515\n')
    self.assertEqual(expected, actual)

  def testDetailIoRenderer(self):
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log_io')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/detail-io-renderer.dat'))
    actual = open('output_dir/detail-io-renderer.dat').readline()
    expected = '12345 3769 1991 1646 387 930 131 5476 1907 5599 6490 625 213\n'
    self.assertEqual(expected, actual)

  def testDetailedCyclerResults(self):
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log_io')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/12345.dat'))
    expected_lines = [['bugzilla.mozilla.org'],
                      ['espn.go.com'],
                      ['slashdot.com']]
    data = [391, 281, 516, 812, 625, 328, 579, 391, 438]
    mean_and_stdd = chromium_utils.FilteredMeanAndStandardDeviation(data)

    for index in range(len(expected_lines)):
      data_for_page = data[(index * 3):(index * 3 + 3)]
      page_mean_and_stdd = chromium_utils.FilteredMeanAndStandardDeviation(
          data_for_page)
      expected_lines[index].extend(
          process_log.FormatFloat(x) for x in page_mean_and_stdd)
      expected_lines[index].extend(data_for_page)

    for index in range(len(expected_lines)):
      expected_lines[index] = self._JoinWithSpacesAndNewLine(
          expected_lines[index])

    index = 0
    for line in open('output_dir/12345.dat').readlines():
      self.assertEqual(expected_lines[index], line)
      index += 1
    self.assertEqual(len(expected_lines), index, 'Not enough entries')

  def testUpdateSummaryFile(self):
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log_io')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/summary.dat'))
    actual = open('output_dir/summary.dat').readline()
    all_timings = [391, 812, 579, 281, 625, 391, 516, 328, 438]
    mean_and_stddev = chromium_utils.FilteredMeanAndStandardDeviation(
        [sum(all_timings[:3]), sum(all_timings[3:6]), sum(all_timings[6:])])

    all_timings_reference = [31, 46, 31, 31, 63, 47, 31, 46, 31]
    mean_and_stddev_reference = chromium_utils.FilteredMeanAndStandardDeviation(
        [
          sum(all_timings_reference[:3]),
          sum(all_timings_reference[3:6]),
          sum(all_timings_reference[6:])
        ]
    )

    data = [12345]
    data.extend([process_log.FormatFloat(x) for x in mean_and_stddev])
    data.extend(
        [process_log.FormatFloat(x) for x in mean_and_stddev_reference])

    expected = self._JoinWithSpacesAndNewLine(data)
    self.assertEqual(expected, actual)

  def testPrependsSummary(self):
    files_that_are_prepended = ['summary.dat','summary-vm-peak-browser.dat',
                                'summary-vm-peak-renderer.dat',
                                'summary-io-op-browser.dat',
                                'summary-io-byte-browser.dat',
                                'detail-io-browser.dat',
                                'detail-io-renderer.dat']
    os.mkdir('output_dir')
    for filename in files_that_are_prepended:
      filename = os.path.join('output_dir', filename)
      file = open(filename, 'w')
      control_line = 'this is a line one, should become line two'
      file.write(control_line)
      file.close()
      step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                                 'page_cycler_log_io')
      step.commandComplete('mycommand')

      self.assert_(os.path.exists(filename))
      text = open(filename).read()
      self.assert_(len(text.splitlines()) > 1,
                   'File %s was not prepended' % filename)
      self.assertEqual(control_line, text.splitlines()[1],
                       'File %s was not prepended' % filename)

  def testMakeOutputDirCreatesCorrectDirectoryForTheReport(self):
    self._RemoveOutputDir() #slightly different setup
    log_parser = process_log.PageCyclerLogProcessor(output_dir='output_dir')
    log_parser._MakeOutputDirectory()
    self.assert_(os.path.exists('output_dir'))

  def testMakeOutputDirWithNestedDirectoriesSpecified(self):
    self._RemoveOutputDir()
    log_parser = process_log.PageCyclerLogProcessor(
        output_dir='output_dir/subdir')
    log_parser._MakeOutputDirectory()
    self.assert_(os.path.exists('output_dir/subdir'))

  def testMakeOutputDirCreatesWithExistingParentDir(self):
    self._RemoveOutputDir()
    os.mkdir('output_dir')
    log_parser = process_log.PageCyclerLogProcessor(
        output_dir='output_dir/subdir')
    log_parser._MakeOutputDirectory()
    self.assert_(os.path.exists('output_dir/subdir'))

  def testIntlMissingDataDoesNotFailParsing(self):
    """The test just makes sure we cover the code and
    don't fail on missing data in pagecycler intl1 logs."""
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log_io_intl1')
    step.commandComplete('mycommand')

  def testFixIntl1DuplicateEntries(self):
    """This test makes sure that we use first entry in case of duplicate
    values.

    See http://b/issue?id=1031895
    """
    step = self._ConstructStep(process_log.PageCyclerLogProcessor,
                               'page_cycler_log_duplicates')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/summary-vm-peak-renderer.dat'))
    actual = open('output_dir/summary-vm-peak-renderer.dat').readline()
    self.assertEqual('12345 236003328 234422272\n', actual)

    self.assert_(os.path.exists('output_dir/detail-io-renderer.dat'))
    actual = open('output_dir/detail-io-renderer.dat').readline()
    expected = ('12345 37409 31364 314 9845 7779 '
                '5 37850 30060 605 9340 9431 115\n')
    self.assertEqual(expected, actual)


class BenchpressPerformanceTestStepTest(GoogleLoggingStepTest):

  def testPrependsSummaryBenchpress(self):
    files_that_are_prepended = ['summary.dat']
    os.mkdir('output_dir')
    for filename in files_that_are_prepended:
      filename = os.path.join('output_dir', filename)
      file = open(filename, 'w')
      control_line = 'this is a line one, should become line two'
      file.write(control_line)
      file.close()
      step = self._ConstructStep(process_log.BenchpressLogProcessor,
                                'benchpress_log')
      step.commandComplete('mycommand')

      self.assert_(os.path.exists(filename))
      text = open(filename).read()
      self.assert_(len(text.splitlines()) > 1,
                   'File %s was not prepended' % filename)
      self.assertEqual(control_line, text.splitlines()[1],
                       'File %s was not prepended' % filename)

  def testBenchpressSummary(self):
    step = self._ConstructStep(process_log.BenchpressLogProcessor,
                               'benchpress_log')
    step.commandComplete('mycommand')

    self.assert_(os.path.exists('output_dir/summary.dat'))
    actual = open('output_dir/summary.dat').readline()
    expected = '12345 469 165 1306 64 676 38 372 120 232 294 659 1157 397\n'
    self.assertEqual(expected, actual)

  def testCreateReportLink(self):
    log_processor_class = chromium_utils.InitializePartiallyWithArguments(
        process_log.BenchpressLogProcessor, report_link=self._report_link,
        output_dir=self._output_dir)
    step = chromium_step.ProcessLogShellStep(log_processor_class)
    build_mock = pmock.Mock()
    source_mock = pmock.Mock()
    change_mock = pmock.Mock()
    change_mock.revision = self._revision
    source_mock.changes = [change_mock]
    build_mock.source = source_mock
    step_status = pmock.Mock()
    step_status.expects(pmock.once()) \
        .addURL(pmock.eq('results'), pmock.eq(
                                          log_processor_class().ReportLink()))
    step.build = build_mock
    step.step_status = step_status
    step._CreateReportLinkIfNeccessary()
    build_mock.verify()


class PlaybackLogProcessorTest(GoogleLoggingStepTest):
  def testDetails(self):
    step = self._ConstructStep(process_log.PlaybackLogProcessor,
                              'playback_log')

    step.commandComplete('mycommand')
    expected = simplejson.dumps(
      {'12345':
        {
          'reference':
          {
            't:V8.ParseLazy':
            {
              'data': [210, 240, 225]
            },
            'c:V8.TotalExternalStringMemory':
            {
              'data': [3847860, 3845000, 3849000]
            }
          },
          'latest':
          {
            't:V8.ParseLazy':
            {
              'data': [200, 190, 198]
            },
            'c:V8.TotalExternalStringMemory':
            {
              'data': [3847850, 3846900, 3848450]
            }
          }
        }
      }
    )

    self.assert_(os.path.exists('output_dir/details.dat'))

    actual = open('output_dir/details.dat').readline()
    self.assertEqual(expected + '\n', actual)

  def testSummary(self):
    step = self._ConstructStep(process_log.PlaybackLogProcessor,
                              'playback_log')

    step.commandComplete('mycommand')
    expected = simplejson.dumps(
      {'12345':
        {
          'reference':
          {
            't:V8.ParseLazy':
            {
              'mean': '225.00',
              'stdd': '12.25'
            },
            'c:V8.TotalExternalStringMemory':
            {
              'mean': '3847286.67',
              'stdd': '1682.56'
            }
          },
          'latest':
          {
            't:V8.ParseLazy':
            {
              'mean': '196.00',
              'stdd': '4.32'
            },
            'c:V8.TotalExternalStringMemory':
            {
              'mean': '3847733.33',
              'stdd': '638.14'
            }
          }
        }
      }
    )

    self.assert_(os.path.exists('output_dir/summary.dat'))

    actual = open('output_dir/summary.dat').readline()
    self.assertEqual(expected + '\n', actual)


class StartupTestLogProcessorTest(GoogleLoggingStepTest):

  def testStartupSummaryTiming(self):
    step = self._ConstructStep(process_log.StartupTestLogProcessor,
                               'startup_test_log')

    step.commandComplete('mycommand')
    self.assert_(os.path.exists('output_dir/summary.dat'))

    actual = open('output_dir/summary.dat').readline()
    data = [248.54, 226.73, 259.85, 219.63, 205.71]
    reference_data = [225.98, 222.25, 223.09, 230.94, 204.17]
    mean_and_stdd = chromium_utils.FilteredMeanAndStandardDeviation(data)
    reference_mean_and_stdd = chromium_utils.FilteredMeanAndStandardDeviation(
        reference_data)

    expected = [self._revision]
    expected.extend([process_log.FormatFloat(x) for x in mean_and_stdd])
    expected.extend(
        [process_log.FormatFloat(x) for x in reference_mean_and_stdd])

    self.assertEqual(self._JoinWithSpacesAndNewLine(expected), actual)

  def testDestStartupSummaryTiming(self):
    """The difference between startup and dest startup tests is that dest tests
    do not have reference timing"""
    step = self._ConstructStep(process_log.StartupTestLogProcessor,
                               'dest_startup_test_log')

    step.commandComplete('mycommand')
    self.assert_(os.path.exists('output_dir/summary.dat'))
    actual = open('output_dir/summary.dat').readline()
    data = [1443.00, 1357.87, 1435.86, 1283.71, 1426.45]
    mean_and_stdd = chromium_utils.FilteredMeanAndStandardDeviation(data)
    expected = [self._revision]
    expected.extend([process_log.FormatFloat(x) for x in mean_and_stdd])
    expected.extend([process_log.FormatFloat(x) for x in [0, 0]])

    self.assertEqual(self._JoinWithSpacesAndNewLine(expected), actual)

  def testStartupPages(self):
    step = self._ConstructStep(process_log.StartupTestLogProcessor,
                               'startup_test_log')

    step.commandComplete('mycommand')
    pages_file_name = 'output_dir/%s.dat' % self._revision
    self.assert_(os.path.exists(pages_file_name))
    actual = open(pages_file_name).readline()
    expected = " ".join(['about:blank', '248.54', '226.73', '259.85', '219.63',
                         '205.71\n'])
    self.assertEqual(expected, actual)

    def testDestStartupPages(self):
      step = self._ConstructStep(process_log.StartupTestLogProcessor,
                                 'startup_test_log')

      step.commandComplete('mycommand')
      pages_file_name = 'output_dir/%s.dat' % self._revision
      self.assert_(os.path.exists(pages_file_name))
      actual = open(pages_file_name).readline()
      expected = " ".join(['about:blank', '1443.00', '1357.87', '1435.86',
                           '1283.71', '1426.45\n'])
      self.assertEqual(expected, actual)


class TabSwitchingTestLogProcessorTest(GoogleLoggingStepTest):

  def testSummary(self):
    step = self._ConstructStep(process_log.TabSwitchingTestLogProcessor,
                               'tab_switching_test_log')
    step.commandComplete('mycommand')
    self.assert_(os.path.exists('output_dir/summary.dat'))
    actual = open('output_dir/summary.dat').readline()
    data = [3305.8,2704.2]
    expected = [self._revision]
    expected.extend([process_log.FormatFloat(x) for x in data])
    expected.extend([process_log.FormatFloat(x) for x in [0, 0]])

    self.assertEqual(self._JoinWithSpacesAndNewLine(expected), actual)


class GraphingLogProcessorTest(GoogleLoggingStepTest):

  def testSummary(self):
    step = self._ConstructStep(process_log.GraphingLogProcessor,
                               'graphing_processor.log')
    step.commandComplete('mycommand')
    for graph in ('commit_charge', 'ws_final_total', 'vm_final_browser',
                  'vm_final_total', 'ws_final_browser', 'processes'):
      filename = '%s-summary.dat' % graph
      self.assert_(os.path.exists(os.path.join('output_dir', filename)))
      # Since the output files are JSON-encoded, they may differ in form, but
      # represent the same data. Therefore, we decode them before comparing.
      actual = simplejson.load(open(os.path.join('output_dir', filename)))
      expected = simplejson.load(open(os.path.join('data', filename)))
      self.assertEqual(expected, actual)

  def testGraphList(self):
    step = self._ConstructStep(process_log.GraphingLogProcessor,
                               'graphing_processor.log')
    step.commandComplete('mycommand')
    actual_file = os.path.join('output_dir', 'graphs.dat')
    self.assert_(os.path.exists(actual_file))
    actual = simplejson.load(open(actual_file))
    expected = simplejson.load(open(
        os.path.join('data', 'graphing_processor-graphs.dat')))
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  unittest.main()
