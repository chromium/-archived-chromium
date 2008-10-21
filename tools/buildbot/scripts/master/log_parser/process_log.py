#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines various log processors used by buildbot steps.

Current approach is to set an instance of log processor in
the ProcessLogTestStep implementation and it will call process()
method upon completion with full data from process stdio.
"""

import errno
import os
import re
import simplejson

import chromium_config as config
import chromium_utils

READABLE_FILE_PERMISSIONS = int('644', 8)

# For the GraphingLogProcessor, the file into which it will save a list
# of graph names for use by the JS doing the plotting.
GRAPH_LIST = config.Master.perf_graph_list

def FormatFloat(number):
  """Formats float with two decimal points."""
  if number:
    return '%.2f' % number
  else:
    return '0.00'


def Prepend(filename, data):
  chromium_utils.Prepend(filename, data)
  os.chmod(filename, READABLE_FILE_PERMISSIONS)


METRIC_SUFFIX = {-3: 'm', 0: '', 3: 'k', 6: 'M'}

def FormatHumanReadable(number):
  """Formats a float into three significant figures, using metric suffixes.

  Only m, k, and M prefixes (for 1/1000, 1000, and 1,000,000) are used.
  Examples:
    0.0387    => 38.7m
    1.1234    => 1.12
    10866     => 10.8k
    682851200 => 683M
  """
  scientific = '%.2e' % float(number)  # 6.83e+005
  digits = float(scientific[:4])       # 6.83
  exponent = int(scientific[5:])       # int('+005') = 5
  while exponent % 3:
    digits *= 10
    exponent -= 1
  while exponent > 6:
    digits *= 10
    exponent -= 1
  while exponent < -3:
    digits /= 10
    exponent += 1
  if digits >= 100:
    # Don't append a meaningless '.0' to an integer number.
    digits = int(digits)
  # Exponent is now divisible by 3, between -3 and 6 inclusive: (-3, 0, 3, 6).
  return '%s%s' % (digits, METRIC_SUFFIX[exponent])


class PerformanceLogProcessor(object):
  """ Parent class for performance log parsers. """

  def __init__(self, report_link=None, output_dir=None):
    self._report_link = report_link
    if output_dir is None:
      output_dir = os.getcwd()
    elif output_dir.startswith('~'):
      output_dir = os.path.expanduser(output_dir)
    self._output_dir = output_dir
    self._matches = {}

    # The revision isn't known until the Process() call.
    self._revision = -1

  def Process(self, revision, data):
    """Invoked by the step with data from log file once it completes.

    Each subclass needs to override this method to provide custom logic,
    which should include setting self._revision.
    Args:
      revision: changeset revision number that triggered the build.
      data: content of the log file that needs to be processed.

    Returns:
      A list of strings to be added to the waterfall display for this step.
    """
    self._revision = revision
    return []

  def ReportLink(self):
    return self._report_link

  def _ShouldWriteResults(self):
    """Tells whether the results should be persisted.

    Write results if revision number is available for the current
    build and the report link was specified."""
    return self._revision > 0 and self.ReportLink()

  def _JoinWithSpaces(self, array):
    return ' '.join([str(x) for x in array])

  def _JoinWithSpacesAndNewLine(self, array):
    return '%s\n' % self._JoinWithSpaces(array)

  def _MakeOutputDirectory(self):
    if self._output_dir and not os.path.exists(self._output_dir):
      os.makedirs(self._output_dir)


class PageCyclerLogProcessor(PerformanceLogProcessor):

  RESULTS_TYPE_REGEX = re.compile(r'^(__pc.*) = .*')

  RESULTS_REGEX = \
    re.compile(r'^__pc_.* = ((?P<NUM>\d+)|\[(?P<PAGES_OR_TIMING>.*)\]).*$')

  def Process(self, revision, data):
    # Revision may be -1, for a forced build.
    self._revision = revision

    # The text summary wil be built by other methods as we go.
    self._text_summary = []

    self._MakeOutputDirectory()

    for log_line in data.splitlines():
     self._ProcessLine(log_line)

    if '__pc_pages' in self._matches:
      self.__CreateAllPageCyclerTimingData()
      self.__CreateSummaryPageCyclerTimingData()

    if '__pc_browser_vm_peak' in self._matches:
      self.__CreateMemoryPeakData('browser', 'vm')

    if '__pc_render_vm_peak' in self._matches:
      self.__CreateMemoryPeakData('render', 'vm', 'renderer')

    if '__pc_browser_ws_peak' in self._matches:
      self.__CreateMemoryPeakData('browser', 'ws')

    if '__pc_render_ws_peak' in self._matches:
      self.__CreateMemoryPeakData('render', 'ws', 'renderer')

    if '__pc_browser_read_op' in self._matches:
      self.__CreateIoSummaryData('op')
      self.__CreateIoSummaryData('byte')
      self.__CreateIoDetailsData('browser')

    if '__pc_render_read_op' in self._matches:
      self.__CreateIoDetailsData('render', 'renderer')

    return self._text_summary

  def _ProcessLine(self, log_line):
    type_match = PageCyclerLogProcessor.RESULTS_TYPE_REGEX.match(log_line)
    if type_match:
      pattern = type_match.group(1)
      results_match = PageCyclerLogProcessor.RESULTS_REGEX.match(log_line)
      if results_match.groupdict()['NUM'] != None:
        if not pattern in self._matches:
          self._matches[pattern] = int(results_match.group(2))
      elif results_match.groupdict()['PAGES_OR_TIMING'] != None:
        if not pattern in self._matches:
          self._matches[pattern] = results_match.group(3)

  def __CreateAllPageCyclerTimingData(self):
    # If we're not writing a file, don't bother with any parsing; the text
    # summary will be handled by CreateSummaryPageCyclerTimingData.
    if not self._ShouldWriteResults():
      return
    filename = os.path.join(self._output_dir,
                            '%s.dat' % self._revision)
    file = open(filename, 'w')

    page_index = 0
    page_names = self._matches['__pc_pages'].split(',')
    all_pages_timings = self._matches['__pc_timings'].split(',')
    for page_name in page_names:
      page_timings = []
      for run_index in range(self.__PageCyclerIterations()):
        index = run_index * self.__NumberOfPageCyclerPages() + page_index
        page_timings.append(int(all_pages_timings[index]))

      mean_stdd = chromium_utils.FilteredMeanAndStandardDeviation(page_timings)
      results = [page_name] + [FormatFloat(x) for x in mean_stdd]
      results.extend(page_timings)
      file.write(self._JoinWithSpacesAndNewLine(results))
      os.chmod(filename, READABLE_FILE_PERMISSIONS)
      page_index += 1

  def __CreateSummaryPageCyclerTimingData(self):
    all_pages_timing = self._matches['__pc_timings'].split(',')
    timings_by_iterations = self.__TimingsByIterations(all_pages_timing)
    std_dev = chromium_utils.FilteredMeanAndStandardDeviation(
        timings_by_iterations)

    if '__pc_reference_timings' in self._matches:
      all_ref_pages_timing = self._matches['__pc_reference_timings'].split(',')
      timings_by_iterations = self.__TimingsByIterations(all_ref_pages_timing)
      std_dev_ref = chromium_utils.FilteredMeanAndStandardDeviation(
          timings_by_iterations)

    else:
      std_dev_ref = [0, 0]

    std_dev = [FormatFloat(x) for x in std_dev]
    std_dev_ref = [FormatFloat(x) for x in std_dev_ref]

    if self._ShouldWriteResults():
      results = [self._revision]
      results.extend(std_dev)
      results.extend(std_dev_ref)
      results = self._JoinWithSpacesAndNewLine(results)
      filename = os.path.join(self._output_dir, 'summary.dat')
      Prepend(filename, results)

    # Show means.
    self._text_summary.append('t: %s (%s)' %
        (FormatHumanReadable(std_dev[0]), FormatHumanReadable(std_dev_ref[0])))


  def __CreateMemoryPeakData(self, process, memory_type, file_name=None):
    """ Writes peak memory data for a given process.
    Args:
      process: process ("browser" or "render" [sic]) for which memory data
          is to be written
      memory_type: type of memory measurement ('vm' or 'ws' (working set))
      file_name: use only if file name uses different pattern from type
          E.g. For type 'render', pass 'renderer' so the filename used is
          'summary-vm-peak-renderer.dat'.
    """
    if not file_name:
      file_name = process

    memory = self._matches['__pc_%s_%s_peak' % (process, memory_type)]
    memory_ref =  self._matches['__pc_reference_%s_%s_peak' % (process,
                                                               memory_type)]

    # Write the summary data file, for graphing.
    if self._ShouldWriteResults():
      results = self._JoinWithSpacesAndNewLine(
          [self._revision, memory, memory_ref])
      filename = os.path.join(self._output_dir,
                              'summary-%s-peak-%s.dat' % (memory_type,
                                                          file_name))
      Prepend(filename, results)

    # Add info to the waterfall display.
    self._text_summary.append('%s-%s: %s (%s)' %
        (memory_type, process[0],
         FormatHumanReadable(memory), FormatHumanReadable(memory_ref)))

  def __CreateIoSummaryData(self, type):
    """ Writes IO Summary data of type "byte" or "op"
    Args:
      type: IO data for specific type ("byte" or "op").
    """
    read = self._matches['__pc_browser_read_%s' % type]
    write = self._matches['__pc_browser_write_%s' % type]
    other = self._matches['__pc_browser_other_%s' % type]
    total = read + write + other
    read_reference = self._matches['__pc_reference_browser_read_%s'
                                   % type]
    write_reference = self._matches['__pc_reference_browser_write_%s'
                                    % type]
    other_reference = self._matches['__pc_reference_browser_other_%s'
                                    % type]
    total_reference = read_reference + \
                      write_reference + other_reference
    if self._ShouldWriteResults():
      results = self._JoinWithSpacesAndNewLine(
          [self._revision, total, total_reference])
      filename = os.path.join(self._output_dir,
                              'summary-io-%s-browser.dat' % type)
      Prepend(filename, results)
    self._text_summary.append('io-%s: %s (%s)' % (type[:4],
                                       FormatHumanReadable(total),
                                       FormatHumanReadable(total_reference)))

  def __CreateIoDetailsData(self, type, file_type=None):
    """ Writes IO details of type "browser" or "render"
    Args:
      type: IO data for specific type ("browser" or "render")
      file_type: use only if file name uses different pattern than type
        E.g. For type 'render', pass 'renderer' so the filename used is
        'detail-io-renderer.dat'.
    """
    # If we're not writing a file, don't bother with any parsing.
    if not self._ShouldWriteResults():
      return
    if not file_type:
      file_type = type

    data = [self._revision]
    detail_keys = (
        '__pc_%s_read_op' % type,
        '__pc_%s_write_op' % type,
        '__pc_%s_other_op' % type,
        '__pc_%s_read_byte' % type,
        '__pc_%s_write_byte' % type,
        '__pc_%s_other_byte' % type,
        '__pc_reference_%s_read_op' % type,
        '__pc_reference_%s_write_op' % type,
        '__pc_reference_%s_other_op' % type,
        '__pc_reference_%s_read_byte' % type,
        '__pc_reference_%s_write_byte' % type,
        '__pc_reference_%s_other_byte' % type)

    data.extend(self.__DetailsByKeys(detail_keys))
    results = self._JoinWithSpacesAndNewLine(data)
    filename = os.path.join(self._output_dir,
                            'detail-io-%s.dat' % file_type)
    Prepend(filename, results)

  def __PageCyclerIterations(self):
    if not hasattr(self, '__iterations'):
      self.__iterations = (len(self._matches['__pc_timings'].split(',')) /
                          self.__NumberOfPageCyclerPages())
    return self.__iterations

  def __NumberOfPageCyclerPages(self):
    if not hasattr(self, '__page_cycler_pages_count'):
      self.__page_cycler_pages_count = len(self._matches['__pc_pages'].
                                           split(','))
    return self.__page_cycler_pages_count

  def __TimingsByIterations(self, all_pages_timing):
    all_iterations_timings = []
    for iteration_index in range(self.__PageCyclerIterations()):
      timings_by_iteration = []
      for page_index in range(self.__NumberOfPageCyclerPages()):
        index = self.__NumberOfPageCyclerPages() * iteration_index + page_index
        timing = int(all_pages_timing[index])
        timings_by_iteration.append(timing)
      all_iterations_timings.append(sum(timings_by_iteration))

    return all_iterations_timings

  def __DetailsByKeys(self, detail_keys):
    """Looks up data for each key in the details and if found
    adds it to the array that will be returned. Otherwise it
    ignores the key."""
    data = []
    for key in detail_keys:
      if key in self._matches:
        data.append(self._matches[key])
    return data

class BenchpressLogProcessor(PerformanceLogProcessor):

  TIMING_REGEX = re.compile(r'.*Time \(([\w\d]+)\): (\d+)')

  def Process(self, revision, data):
    # Revision may be -1, for a forced build.
    self._revision = revision

    # The text summary wil be built by other methods as we go.
    self._text_summary = []

    self._MakeOutputDirectory()
    for log_line in data.splitlines():
      self._ProcessLine(log_line)
    self._WriteResultsToSummaryFile()
    return self._text_summary

  def _ProcessLine(self, log_line):
    if log_line.find('Time (') > -1:
      match = BenchpressLogProcessor.TIMING_REGEX.match(log_line)
      self._matches[match.group(1)] = int(match.group(2))

  def _WriteResultsToSummaryFile(self):
    algorithms = ['Fibonacci', 'Loop', 'Towers', 'Sieve', 'Permute', 'Queens',
                   'Recurse', 'Sum', 'BubbleSort', 'QuickSort', 'TreeSort',
                   'Tak', 'Takl']
    results = [self._revision]
    for algorithm in algorithms:
      results.append(self._matches[algorithm])

    if self._ShouldWriteResults():
      filename = os.path.join(self._output_dir, 'summary.dat')
      Prepend(filename, self._JoinWithSpacesAndNewLine(results))

    # TODO(pamg): append an appropriate metric to the waterfall display if
    # we start running these tests again.
    # self._text_summary.append(...)


class PlaybackLogProcessor(PerformanceLogProcessor):
  """Log processor for playback results.

  Parses results and outputs results to file in JSON format.
  """

  LATEST_START_LINE = '=LATEST='
  REFERENCE_START_LINE = '=REFERENCE='

  RESULTS_STARTING_LINE = '<stats>'
  RESULTS_ENDING_LINE = '</stats>'

  # Matches stats counter output.  Examples:
  # c:WebFrameActiveCount: 3
  # t:WebFramePaintTime: 451
  # c:V8.OsMemoryAllocated: 3887400
  # t:V8.Parse: 159
  #
  # "c" is used to denote a counter, and "t" is used to denote a timer.
  RESULT_LINE = re.compile(r'^((?:c|t):[^:]+):\s*(\d+)')

  def Process(self, revision, data):
    """Does the actual log data processing.

    The data format follows this rule:
      {revision:
        {type:
          {testname:
            {
              'mean': test run mean time/count (only in summary.dat),
              'stdd': test run standard deviation (only in summary.dat),
              'data': raw test data for each run (only in details.dat)
            }
          }
        }
      }
    and written in one line by prepending to details.dat and summary.dat files
    in JSON format, where type is either "latest" or "reference".
    """

    # Revision may be -1, for a forced build.
    self._revision = revision

    self._MakeOutputDirectory()
    should_record = False
    summary_data = {}
    current_type_data = None
    for line in data.splitlines():
      line = line.strip()

      if line == PlaybackLogProcessor.LATEST_START_LINE:
        summary_data['latest'] = {}
        current_type_data = summary_data['latest']
      elif line == PlaybackLogProcessor.REFERENCE_START_LINE:
        summary_data['reference'] = {}
        current_type_data = summary_data['reference']

      if not should_record:
        if PlaybackLogProcessor.RESULTS_STARTING_LINE == line:
          should_record = True
        else:
          continue
      else:
        if PlaybackLogProcessor.RESULTS_ENDING_LINE == line:
          should_record = False
          continue

      if current_type_data != None:
        match = PlaybackLogProcessor.RESULT_LINE.match(line)
        if match:
          test_name = match.group(1)
          test_data = int(match.group(2))

          if current_type_data.get(test_name, {}) == {}:
            current_type_data[test_name] = {}

          if current_type_data[test_name].get('data', []) == []:
            current_type_data[test_name]['data'] = []

          current_type_data[test_name]['data'].append(test_data)

    # Only proceed if they both passed.
    if (summary_data.get('latest', {}) != {} and
        summary_data.get('reference', {}) != {}):
      # Write the details file, which contains the raw results.
      if self._ShouldWriteResults():
        filename = os.path.join(self._output_dir, 'details.dat')
        Prepend(filename, simplejson.dumps({revision : summary_data}) + '\n')

      for type in summary_data.keys():
        for test_name in summary_data[type].keys():
          test = summary_data[type][test_name]
          mean, stdd = chromium_utils.MeanAndStandardDeviation(test['data'])
          test['mean'] = FormatFloat(mean)
          test['stdd'] = FormatFloat(stdd)
          # Remove test data as it is not needed in the summary file.
          del test['data']

      # Write the summary file, which contains the mean/stddev (data necessary
      # to draw the graphs).
      if self._ShouldWriteResults():
        filename = os.path.join(self._output_dir, 'summary.dat')
        Prepend(filename, simplejson.dumps({revision : summary_data}) + '\n')
    return []


class BasicTestLogProcessor(PerformanceLogProcessor):
  """Base class for simple log processors such as the ones for Startup,
  NewTabUIStartup, and TabSwitching tests.
  """

  __timings = []
  __reference_timings = []

  def _GetTimings(self):
    return self.__timings

  def _GetReferenceTimings(self):
    return self.__reference_timings

  def _GetMeanAndStandardDeviation(self, timings):
    return chromium_utils.FilteredMeanAndStandardDeviation(timings)

  def Process(self, revision, data):
    """Does the actual log data processing.

    Prepends a line to summary.dat with revision number, timing, sample
    standard deviation, reference timing, and reference sample standard
    deviation.
    """
    # Revision may be -1, for a forced build.
    self._revision = revision

    # The text summary wil be built by other methods as we go.
    self._text_summary = []

    self._MakeOutputDirectory()

    self._ProcessLines(self._GetLineRegExp(), self._GetResultsRegExp(), data)

    if len(self.__timings) > 0:
      self.__timings = [float(x) for x in self.__timings]
      self.__reference_timings = [float(x) for x in self.__reference_timings]

      self._WriteSummary()
      self._WriteEverythingElse()

    return self._text_summary

  def _ProcessLines(self, line_regexp, regexp, data):
    """Processes each line in a list of data, storing the results.
    """
    for line in data.splitlines():
      if line_regexp.match(line):
        self._StoreTimings(regexp, line)

  def _StoreTimings(self, regexp, line):
    """Store the data we are interested in, that is timing and reference
    timing."""
    match = regexp.match(line)
    # we are ignoring 'pages' values: it is always 'about:blank'
    if match.group(1) == 'timings':
      self.__timings = match.group(2).split(',')
    elif match.group(1) == 'reference_timings':
      self.__reference_timings = match.group(2).split(',')

  def _GetLineRegExp(self):
    """Get the regular expression that matches a line of potential results.
    Should be overridden by derived classes.

    See StartupTestLogProcessor and TabSwitchingTestLogProcessor for examples.
    """
    raise NotImplementedError()

  def _GetResultsRegExp(self):
    """Get the regular expression that matches the timing results of a test.
    Should be overridden by derived classes.

    See StartupTestLogProcessor and TabSwitchingTestLogProcessor for examples.
    """
    raise NotImplementedError()

  def _WriteSummary(self):
    """Writes mean and sample standard deviation for timings and reference
    timings to summary.dat file."""
    std_dev = self._GetMeanAndStandardDeviation(self.__timings)
    if len(self.__reference_timings) > 0:
      std_dev_ref = self._GetMeanAndStandardDeviation(self.__reference_timings)
    else:
      std_dev_ref = [0, 0]

    if self._ShouldWriteResults():
      results = [self._revision]
      results.extend([FormatFloat(x) for x in std_dev])
      results.extend([FormatFloat(x) for x in std_dev_ref])
      results = self._JoinWithSpacesAndNewLine(results)
      filename = os.path.join(self._output_dir, 'summary.dat')
      Prepend(filename, results)

    # Show means.
    self._text_summary.append('t: %s (%s)' %
        (FormatHumanReadable(std_dev[0]), FormatHumanReadable(std_dev_ref[0])))

  def _WriteEverythingElse(self):
    """Writes everything other than the summary (e.g. any detailed
    statistics).  By default nothing is written, by derived classes may
    override this function to their liking.
    """
    pass


class StartupTestLogProcessor(BasicTestLogProcessor):
  """Log processor for Startup and NewTabUIStartup tests."""

  RESULTS_LINE_REGEX = re.compile(r'^__ts.*')
  RESULTS_REGEX = re.compile(r'^__ts_(.*) = \[(.*)\]')

  def _GetLineRegExp(self):
    return StartupTestLogProcessor.RESULTS_LINE_REGEX

  def _GetResultsRegExp(self):
    return StartupTestLogProcessor.RESULTS_REGEX

  def _WriteEverythingElse(self):
    self._WritePageTimings()

  def _WritePageTimings(self):
    """Writes details of timings to revision file."""
    if not self._ShouldWriteResults():
      return
    filename = os.path.join(self._output_dir, '%s.dat' % self._revision)
    data = ['about:blank']
    data.extend(self._GetTimings())
    results = self._JoinWithSpacesAndNewLine(data)
    Prepend(filename, results)


class TabSwitchingTestLogProcessor(BasicTestLogProcessor):
  """Log processor for TabSwitching test."""

  RESULTS_LINE_REGEX = re.compile(r'^__tsw.*')
  RESULTS_REGEX = re.compile(r'^__tsw_(.*) = \[(.*)\]')

  def _GetLineRegExp(self):
    return TabSwitchingTestLogProcessor.RESULTS_LINE_REGEX

  def _GetResultsRegExp(self):
    return TabSwitchingTestLogProcessor.RESULTS_REGEX

  def _GetMeanAndStandardDeviation(self, timings):
    # The output of this test is the mean and standard deviation.
    return timings


class Trace(object):
  """Encapsulates the data needed for one trace on a performance graph."""
  def __init__(self):
    self.important = False
    self.value = 0.0
    self.stddev = 0.0

  def __str__(self):
    result = FormatHumanReadable(self.value)
    if self.stddev:
      result += '+/-%s' % FormatHumanReadable(self.stddev)


class Graph(object):
  """Encapsulates the data needed for one performance graph."""
  def __init__(self):
    self.units = None
    self.traces = {}

  def IsImportant(self):
    """A graph is 'important' if any of its traces is."""
    for trace in self.traces.itervalues():
      if trace.important:
        return True
    return False


class JSONGraphEncoder(simplejson.JSONEncoder):
  def default(self, obj):
    if isinstance(obj, Graph):
      return {'units': obj.units, 'traces': obj.traces}
    if isinstance(obj, Trace):
      # NB: We do not encode the 'important' value: the plot doesn't need it.
      return [obj.value, obj.stddev]
    return simplejson.JSONEncoder.default(self, obj)


class GraphingLogProcessor(PerformanceLogProcessor):
  """Parent class for any log processor expecting standard data to be graphed.

  The log will be parsed looking for any lines of the form
    <*>RESULT <graph_name>: <trace_name>= <value> <units>
  or
    <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...] <units>
  or
    <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  For example,
    *RESULT vm_final_browser: OneTab= 8488 kb
    RESULT startup: reference= [167.00,148.00,146.00,142.00] msec
  The leading * is optional; if it's present, the data from that line will be
  included in the waterfall display. If multiple values are given in [ ], their
  mean and (sample) standard deviation will be written; if only one value is
  given, that will be written. A trailing comma is permitted in the list of
  values.
  Any of the <fields> except <value> may be empty, in which case
  not-terribly-useful defaults will be used. The <graph_name> and <trace_name>
  should not contain any spaces, colons (:) nor equals-signs (=). Furthermore,
  the <trace_name> will be used on the waterfall display, so it should be kept
  short.  If the trace_name ends with '_ref', it will be interpreted as a
  reference value, and shown alongside the corresponding main value on the
  waterfall.
  """
  RESULTS_REGEX = re.compile(
      r'(?P<IMPORTANT>\*)?RESULT '
       '(?P<GRAPH>[^:]*): (?P<TRACE>[^=]*)= '
       '(?P<VALUE>[\{\[]?[\d\., ]+[\}\]]?)( ?(?P<UNITS>.+))?')

  def Process(self, revision, data):
    # Revision may be -1, for a forced build.
    self._revision = revision

    # The text summary will be built by other methods as we go.
    self._text_summary = []

    # A dict of Graph objects, by name.
    self._graphs = {}

    self._MakeOutputDirectory()

    # Parse the log and fill _graphs.
    for log_line in data.splitlines():
      self._ProcessLine(log_line)

    self.__CreateSummaryOutput()
    self.__SaveGraphInfo()

  def _ProcessLine(self, line):
    line_match = self.RESULTS_REGEX.match(line)
    if line_match:
      match_dict = line_match.groupdict()
      graph_name = match_dict['GRAPH'].strip()
      trace_name = match_dict['TRACE'].strip()

      graph = self._graphs.get(graph_name, Graph())
      graph.units = match_dict['UNITS'] or ''
      trace = graph.traces.get(trace_name, Trace())
      trace.value = match_dict['VALUE']
      trace.important = match_dict['IMPORTANT'] or False

      # Compute the mean and standard deviation for a multiple-valued item,
      # or the numerical value of a single-valued item.
      if trace.value.startswith('['):
        value_list = [float(x) for x in trace.value.strip('[],').split(',')]
        trace.value, trace.stddev = self._CalculateStatistics(value_list)
      elif trace.value.startswith('{'):
        stripped = trace.value.strip('{},')
        trace.value, trace.stddev = [float(x) for x in stripped.split(',')]
      else:
        trace.value = float(trace.value)

      graph.traces[trace_name] = trace
      self._graphs[graph_name] = graph

  def _CalculateStatistics(self, value_list):
    """Returns a tuple (mean, standard deviation) from a list of values.

    This method may be overridden by subclasses wanting a different standard
    deviation calcuation (or some other sort of error value entirely).
    """
    return chromium_utils.FilteredMeanAndStandardDeviation(value_list)

  def __CreateSummaryOutput(self):
    """Write the summary data file and collect the waterfall display text.

    The summary file contains JSON-encoded data.

    The waterfall contains lines for each important trace, in the form
      tracename: value< (refvalue)>
    """
    for graph_name, graph in self._graphs.iteritems():
      # Write a line in the applicable summary file for each graph.
      filename = os.path.join(self._output_dir, "%s-summary.dat" % graph_name)
      json = simplejson.dumps({'rev': self._revision,
                               'traces': graph.traces}, cls=JSONGraphEncoder)
      Prepend(filename, json + "\n")

      for trace_name, trace in graph.traces.iteritems():
        # Add a line to the waterfall for each important trace.
        if trace_name.endswith("_ref"):
          continue
        if trace.important:
          display = "%s: %s" % (trace_name, FormatHumanReadable(trace.value))
          if graph.traces.get(trace_name + '_ref'):
            display += "(%s)" % FormatHumanReadable(
                                graph.traces[trace_name + '_ref'].value)
          self._text_summary.append(display)

    self._text_summary.sort()

  def __SaveGraphInfo(self):
    """Keep a list of all graphs ever produced, for use by the plotter.

    Build a list of dictionaries:
      [{'name': graph_name, 'important': important, 'units': units},
       ...,
      ]
    sorted by importance (important graphs first) and then graph_name.
    Save this list into the GRAPH_LIST file for use by the plotting
    page. (We can't just use a plain dictionary with the names as keys,
    because dictionaries are inherently unordered.)
    """
    try:
      graph_file = open(os.path.join(self._output_dir, GRAPH_LIST))
      graph_list = simplejson.load(graph_file)
    except IOError, e:
      if e.errno != errno.ENOENT:
        raise
      graph_file = None
      graph_list = []
    if graph_file:
      graph_file.close()

    # Remove any graphs from the loaded list whose names match ones we just
    # produced, so the new information overwrites the old.
    graph_list = [x for x in graph_list if x['name'] not in self._graphs]

    # Add the graphs we're creating now to the list.
    for graph_name, graph in self._graphs.iteritems():
      graph_list.append({'name': graph_name,
                         'important': graph.IsImportant(),
                         'units': graph.units})

    # Sort by not-'important', since True > False, then by graph_name.
    graph_list.sort(lambda x, y: cmp((not x['important'], x['name']),
                                     (not y['important'], y['name'])))


    graph_file = open(os.path.join(self._output_dir, GRAPH_LIST), 'w')
    simplejson.dump(graph_list, graph_file)
    graph_file.close()

class GraphingPageCyclerLogProcessor(GraphingLogProcessor):
  """Handles additional processing for page-cycler timing data."""

  _page_count = 1
  PAGES_REGEXP = re.compile(r'^Pages: \[(?P<LIST>.*)\]')

  def _ProcessLine(self, line):
    """Also looks for the Pages: line to find the page count."""
    line_match = self.PAGES_REGEXP.match(line)
    if line_match:
      page_list = line_match.groupdict()['LIST'].strip()
      self._page_count = max(1, len(page_list.split(',')))
    else:
      GraphingLogProcessor._ProcessLine(self, line)

  def _CalculateStatistics(self, value_list):
    """Sums the timings over all pages for each iteration and returns a tuple
    (mean, standard deviation) of those sums.
    """
    sums = []
    iteration_count = len(value_list) / self._page_count
    for iteration in range(iteration_count):
      start = self._page_count * iteration
      end = start + self._page_count
      sums += [sum(value_list[start:end])]
    return chromium_utils.FilteredMeanAndStandardDeviation(sums)
