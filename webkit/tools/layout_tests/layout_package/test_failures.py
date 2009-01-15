# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes for failures that occur during tests."""

import path_utils

class FailureSort(object):
  """A repository for failure sort orders and tool to facilitate sorting."""

  # Each failure class should have an entry in this dictionary. Sort order 1
  # will be sorted first in the list. Failures with the same numeric sort
  # order will be sorted alphabetically by Message().
  SORT_ORDERS = {
    'FailureTextMismatch': 1,
    'FailureSimplifiedTextMismatch': 2,
    'FailureImageHashMismatch': 3,
    'FailureTimeout': 4,
    'FailureCrash': 5,
    'FailureMissingImageHash': 6,
    'FailureMissingImage': 7,
    'FailureMissingResult': 8,
  }

  @staticmethod
  def SortOrder(failure_type):
    """Returns a tuple of the class's numeric sort order and its message."""
    order = FailureSort.SORT_ORDERS.get(failure_type.__name__, -1)
    return (order, failure_type.Message())


class TestFailure(object):
  """Abstract base class that defines the failure interface."""
  @staticmethod
  def Message():
    """Returns a string describing the failure in more detail."""
    raise NotImplemented

  def ResultHtmlOutput(self, filename):
    """Returns an HTML string to be included on the results.html page."""
    raise NotImplemented

  def ShouldKillTestShell(self):
    """Returns True if we should kill the test shell before the next test."""
    return False


class FailureWithType(TestFailure):
  """Base class that produces standard HTML output based on the test type.

  Subclasses may commonly choose to override the ResultHtmlOutput, but still
  use the standard OutputLinks.
  """
  def __init__(self, test_type):
    TestFailure.__init__(self)
    self._test_type = test_type

  # Filename suffixes used by ResultHtmlOutput.
  OUT_FILENAMES = []

  def OutputLinks(self, filename, out_names):
    """Returns a string holding all applicable output file links.

    Args:
      filename: the test filename, used to construct the result file names
      out_names: list of filename suffixes for the files. If three or more
          suffixes are in the list, they should be [actual, expected, diff,
          wdiff].
          Two suffixes should be [actual, expected], and a single item is the
          [actual] filename suffix.  If out_names is empty, returns the empty
          string.
    """
    links = ['']
    uris = [self._test_type.RelativeOutputFilename(filename, fn)
            for fn in out_names]
    if len(uris) > 1:
      links.append("<a href='%s'>expected</a>" % uris[1])
    if len(uris) > 0:
      links.append("<a href='%s'>actual</a>" % uris[0])
    if len(uris) > 2:
      links.append("<a href='%s'>diff</a>" % uris[2])
    if len(uris) > 3:
      links.append("<a href='%s'>wdiff</a>" % uris[3])
    return ' '.join(links)

  def ResultHtmlOutput(self, filename):
    return self.Message() + self.OutputLinks(filename, self.OUT_FILENAMES)


class FailureTimeout(TestFailure):
  """Test timed out.  We also want to restart the test shell if this
  happens."""
  @staticmethod
  def Message():
    return "Test timed out"

  def ResultHtmlOutput(self, filename):
    return "<strong>%s</strong>" % self.Message()

  def ShouldKillTestShell(self):
    return True


class FailureCrash(TestFailure):
  """Test shell crashed."""
  @staticmethod
  def Message():
    return "Test shell crashed"

  def ResultHtmlOutput(self, filename):
    # TODO(tc): create a link to the minidump file
    return "<strong>%s</strong>" % self.Message()

  def ShouldKillTestShell(self):
    return True


class FailureMissingResult(FailureWithType):
  """Expected result was missing."""
  OUT_FILENAMES = ["-actual-win.txt"]

  @staticmethod
  def Message():
    return "No expected results found"

  def ResultHtmlOutput(self, filename):
    return ("<strong>%s</strong>" % self.Message() +
            self.OutputLinks(filename, self.OUT_FILENAMES))


class FailureTextMismatch(FailureWithType):
  """Text diff output failed."""
  # Filename suffixes used by ResultHtmlOutput.
  OUT_FILENAMES = ["-actual-win.txt", "-expected.txt", "-diff-win.txt"]
  OUT_FILENAMES_WDIFF = ["-actual-win.txt", "-expected.txt", "-diff-win.txt",
                         "-wdiff-win.html"]

  def __init__(self, test_type, has_wdiff):
    FailureWithType.__init__(self, test_type)
    if has_wdiff:
      self.OUT_FILENAMES = self.OUT_FILENAMES_WDIFF

  @staticmethod
  def Message():
    return "Text diff mismatch"


class FailureSimplifiedTextMismatch(FailureTextMismatch):
  """Simplified text diff output failed.

  The results.html output format is basically the same as regular diff
  failures (links to expected, actual and diff text files) so we share code
  with the FailureTextMismatch class.
  """

  OUT_FILENAMES = ["-simp-actual-win.txt", "-simp-expected.txt",
                   "-simp-diff-win.txt"]
  def __init__(self, test_type):
    # Don't run wdiff on simplified text diffs.
    FailureTextMismatch.__init__(self, test_type, False)

  @staticmethod
  def Message():
    return "Simplified text diff mismatch"


class FailureMissingImageHash(FailureWithType):
  """Actual result hash was missing."""
  # Chrome doesn't know to display a .checksum file as text, so don't bother
  # putting in a link to the actual result.
  OUT_FILENAMES = []

  @staticmethod
  def Message():
    return "No expected image hash found"

  def ResultHtmlOutput(self, filename):
    return "<strong>%s</strong>" % self.Message()


class FailureMissingImage(FailureWithType):
  """Actual result image was missing."""
  OUT_FILENAMES = ["-actual-win.png"]

  @staticmethod
  def Message():
    return "No expected image found"

  def ResultHtmlOutput(self, filename):
    return ("<strong>%s</strong>" % self.Message() +
            self.OutputLinks(filename, self.OUT_FILENAMES))


class FailureImageHashMismatch(FailureWithType):
  """Image hashes didn't match."""
  OUT_FILENAMES = ["-actual-win.png", "-expected.png", "-diff-win.png"]

  @staticmethod
  def Message():
    # We call this a simple image mismatch to avoid confusion, since we link
    # to the PNGs rather than the checksums.
    return "Image mismatch"

class FailureFuzzyFailure(FailureWithType):
  """Image hashes didn't match."""
  OUT_FILENAMES = ["-actual-win.png", "-expected.png"]

  @staticmethod
  def Message():
    return "Fuzzy image match also failed"
