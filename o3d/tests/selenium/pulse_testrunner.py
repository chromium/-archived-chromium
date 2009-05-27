#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Test runner that displays customized results for pulse.

Instead of the usual '.', 'E', and 'F' symbols, more detailed
results are printed after each test.

ie. 'TESTRESULT $TESTNAME: {PASS/FAIL}'
"""



import sys
import time
import unittest


# Disable lint errors about functions having to start with upper case.
# This is done to standardize the case of all functions in this class.
# pylint: disable-msg=C6409
class _PulseTestResult(unittest.TestResult):
  """A specialized class that prints formatted text results to a stream.

  Test results are formatted to be recognized in pulse.
  Used by PulseTestRunner.
  """
  separator1 = "=" * 70
  separator2 = "-" * 70

  def __init__(self, stream, descriptions, verbosity, browser):
    unittest.TestResult.__init__(self)
    self.stream = stream
    self.show_all = verbosity > 1
    self.dots = verbosity == 1
    self.descriptions = descriptions
    # Dictionary of start times
    self.start_times = {}
    # Dictionary of results
    self.results = {}
    self.browser = browser

  def getDescription(self, test):
    """Gets description of test."""
    if self.descriptions:
      return test.shortDescription() or str(test)
    else:
      return str(test)

  def startTest(self, test):
    """Starts test."""
    # Records the start time
    self.start_times[test] = time.time()
    # Default testresult if success not called
    self.results[test] = "FAIL"
    unittest.TestResult.startTest(self, test)
    if self.show_all:
      self.stream.write(self.getDescription(test))
      self.stream.write(" ... ")

  def stopTest(self, test):
    """Called when test is ended."""
    time_taken = time.time() - self.start_times[test]
    result = self.results[test]
    self.stream.writeln("SELENIUMRESULT %s <%s> [%.3fs]: %s"
                        % (test, self.browser, time_taken, result))

  def addSuccess(self, test):
    """Adds success result to TestResult."""
    unittest.TestResult.addSuccess(self, test)
    self.results[test] = "PASS"

  def addError(self, test, err):
    """Adds error result to TestResult."""
    unittest.TestResult.addError(self, test, err)
    self.results[test] = "FAIL"

  def addFailure(self, test, err):
    """Adds failure result to TestResult."""
    unittest.TestResult.addFailure(self, test, err)
    self.results[test] = "FAIL"

  def printErrors(self):
    """Prints all errors and failures."""
    if self.dots or self.show_all:
      self.stream.writeln()
    self.printErrorList("ERROR", self.errors)
    self.printErrorList("FAIL", self.failures)

  def printErrorList(self, flavour, errors):
    """Prints a given list of errors."""
    for test, err in errors:
      self.stream.writeln(self.separator1)
      self.stream.writeln("%s: %s" % (flavour, self.getDescription(test)))
      self.stream.writeln(self.separator2)
      self.stream.writeln("%s" % err)


class PulseTestRunner(unittest.TextTestRunner):
  """A specialized test runner class that displays results in textual form.

  Test results are formatted to be recognized in pulse.
  """

  def __init__(self, stream=sys.stderr, descriptions=1, verbosity=1):
    self.browser = "default_browser"
    unittest.TextTestRunner.__init__(self, stream, descriptions, verbosity)

  def setBrowser(self, browser):
    """Sets the browser name."""
    self.browser = browser

  def _makeResult(self):
    """Returns a formatted test result for pulse."""
    return _PulseTestResult(self.stream,
                            self.descriptions,
                            self.verbosity,
                            self.browser)

if __name__ == "__main__":
  pass
