#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A buildbot command for running and interpreting webkit layout tests."""


import re
from buildbot.process import buildstep
from buildbot.steps import shell
from buildbot.status import builder

class TestObserver(buildstep.LogLineObserver):
  """This class knows how to understand webkit test output."""

  def __init__(self):
    buildstep.LogLineObserver.__init__(self)

    # State tracking for log parsing.
    self._current_category = ''

    # List of unexpectedly failing tests.
    self.failed_tests = []

    # Did we see any unexpectedly passing tests?
    self.had_unexpected_passing = False

    # Headers and regular expressions for parsing logs.  We don't
    # distinguish among failures, crashes, and hangs in the display.
    self._passing_start = re.compile('Expected to .+, but passed')
    self._regressions_start = re.compile('(Regressions: Unexpected'
                                         '|Missing expected results) .+')
    self._section_end = '-' * 78

    self._test_path_line = re.compile('  (\S+)')

  def outLineReceived(self, line):
    """This is called once with each line of the test log."""

    # Are we starting or ending a new section?
    if line.startswith(self._section_end):
      self._current_category = ''
      return

    results = self._passing_start.match(line)
    if results:
      self._current_category = 'passing'
      self.had_unexpected_passing = True
      return

    results = self._regressions_start.match(line)
    if results:
      self._current_category = 'regressions'
      return

    # Are we collecting the list of regressions?
    if self._current_category == 'regressions':
      results = self._test_path_line.match(line)
      if results:
        self.failed_tests.append(results.group(1))


class WebKitCommand(shell.ShellCommand):
  """Buildbot command that knows how to display layout test output."""

  def __init__(self, **kwargs):
    shell.ShellCommand.__init__(self, **kwargs)
    self.test_observer = TestObserver()
    self.addLogObserver('stdio', self.test_observer)
    # The maximum number of failed tests to list on the buildbot waterfall.
    self._MAX_LIST = 25

  def evaluateCommand(self, cmd):
    """Decide whether the command was SUCCESS, WARNINGS, or FAILURE.

    Tests unexpectedly passing is WARNINGS.  Unexpected failures is FAILURE.
    """
    if cmd.rc != 0:
      return builder.FAILURE
    if self.test_observer.had_unexpected_passing:
      return builder.WARNINGS
    return builder.SUCCESS

  def _BasenameFromPath(self, path):
    """Extracts the basename of either a Unix- or Windows- style path,
    assuming it contains either \ or / but not both.
    """
    short_path = path.split('\\')[-1]
    short_path = short_path.split('/')[-1]
    return short_path

  def getText(self, cmd, results):
    if results == builder.SUCCESS:
      return self.describe(True)
    elif results == builder.WARNINGS:
      return self.describe(True) + ['unexpected pass']
    else:
      failures = self.test_observer.failed_tests
      if len(failures) > 0:
        failure_text = ['failed %d' % len(failures)]
        failure_text.append('<div class="BuildResultInfo">')
        # Display test names, with full paths as tooltips.
        for path in failures[:self._MAX_LIST]:
          failure_text.append('<abbr title="%s">%s</abbr>' %
                              (path, self._BasenameFromPath(path)))
        if len(failures) > self._MAX_LIST:
          failure_text.append('...and more')
        failure_text.append('</div>')
      else:
        failure_text = ['crashed or hung']
      return self.describe(True) + failure_text
