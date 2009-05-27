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


"""Selenium tests for the O3D plugin.

Sets up a local Selenium Remote Control server and a static file
server that serves files off the o3d directory.

Launches browsers to test the local build of the o3d plugin
and reports results back to the user.
"""



import os
import platform
import re
import SimpleHTTPServer
import socket
import SocketServer
import subprocess
import sys
import threading
import time
import unittest
import gflags
import javascript_unit_tests
# Import custom testrunner for pulse
import pulse_testrunner
# TODO: figure out if we can remove this hard-coded version
import rev2478_mod as selenium
import samples_tests
import selenium_constants
import selenium_utilities


if platform.system() == "Windows":
  default_java_exe = "java.exe"
else:
  default_java_exe = "java"

# Commands line flags
FLAGS = gflags.FLAGS
gflags.DEFINE_boolean("verbose", False, "verbosity")
gflags.DEFINE_boolean("screenshots", False, "takes screenshots")
gflags.DEFINE_string(
    "java",
    default_java_exe,
    "specifies the path to the java executable.")
gflags.DEFINE_string(
    "selenium_server",
    "",
    "specifies the path to the selenium server jar.")
gflags.DEFINE_string(
    "screencompare",
    "",
    "specifies the directory in which perceptualdiff resides.\n"
    "compares screenshots with reference images")
gflags.DEFINE_string(
    "screenshotsdir",
    selenium_constants.DEFAULT_SCREENSHOT_PATH,
    "specifies the directory in which screenshots will be stored.")
gflags.DEFINE_string(
    "referencedir",
    selenium_constants.DEFAULT_SCREENSHOT_PATH,
    "Specifies the directory where reference images will be read from.")
gflags.DEFINE_string(
    "testprefix", "Test",
    "specifies the prefix of tests to run")
gflags.DEFINE_string(
    "testsuffixes", "",
    "specifies the suffixes, separated by commas of tests to run")
gflags.DEFINE_string(
    "servertimeout", "30",
    "Specifies the timeout value, in seconds, for the selenium server.")

# Browsers to choose from (for browser flag).
# use --browser $BROWSER_NAME to run
# tests for that browser
gflags.DEFINE_list(
    "browser",
    "*firefox",
    "\n".join(["comma-separated list of browsers to test",
               "Options:"] +
              selenium_constants.SELENIUM_BROWSER_SET))
gflags.DEFINE_string(
    "browserpath",
    "",
    "specifies the path to the browser executable "
    "(for platforms that don't support MOZ_PLUGIN_PATH)")

TESTING_ROOT = os.path.abspath(os.path.dirname(__file__) + "/..")


class MyRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  """Hook to handle HTTP server requests.

  Functions as a handler for logging and other utility functions.
  """

  def log_message(self, format, *args):
    """Logging hook for HTTP server."""

    # For now, just suppress logging.
    pass
    # TODO: might be nice to have a verbose option for debugging.


class LocalFileHTTPServer(threading.Thread):
  """Minimal HTTP server that serves local files.

  Members:
    http_alive: event to signal that http server is up and running
    http_port: the TCP port the server is using
  """

  START_PORT = 8100
  END_PORT = 8105

  def __init__(self, local_root=None):
    """Initializes the server.

    Initializes the HTTP server to serve static files from the
    local o3d directory

    Args:
      local_root: all files below this path are served.  If not specified,
        the current directory is the root.
    """
    threading.Thread.__init__(self)
    self._local_root = local_root
    self.http_alive = threading.Event()
    self.http_port = 0

  def run(self):
    """Runs the HTTP server.

    Server is started on an available port in the range of
    START_PORT to END_PORT
    """

    if self._local_root:
      os.chdir(self._local_root)

    for self.http_port in range(self.START_PORT, self.END_PORT):
      # Attempt to start the server
      try:
        httpd = SocketServer.TCPServer(("", self.http_port),
                                       MyRequestHandler)
      except socket.error:
        # Server didn't manage to start up, try another port.
        pass
      else:
        self.http_alive.set()
        httpd.serve_forever()

    if not self.http_alive.isSet():
      print("No available port found for HTTP server in the range %d to %d."
            % (self.START_PORT, self.END_PORT))

  @staticmethod
  def StartServer(local_root=None):
    """Create and start a LocalFileHTTPServer on a separate thread.

    Args:
      local_root: serve all static files below this directory.  If not
        specified, the current directory is the root.

    Returns:
      http_server: LocalFileHTTPServer() object
    """

    # Start up the Selenium Remote Control server
    http_server = LocalFileHTTPServer(local_root)
    http_server.setDaemon(True)
    http_server.start()

    # Wait till the Selenium RC Server is up
    http_server.http_alive.wait()

    print "LocalFileHTTPServer started on port %d" % http_server.http_port

    return http_server


class SeleniumRemoteControl(threading.Thread):
  """A thread that launches the Selenium Remote Control server.

  The Remote Control server allows us to launch a browser and remotely
  control it from a script.

  Members:
    selenium_alive: event to signal that selenium server is up and running
    selenium_port: the TCP port the server is using
    process: the subprocess.Popen instance for the server
  """

  START_PORT = 5430
  END_PORT = 5535

  def __init__(self, verbose, java_path, selenium_server, server_timeout):
    """Initializes the SeleniumRemoteControl class.

    Args:
      verbose: boolean verbose flag
      java_path: path to java used to run selenium.
      selenium_server: path to jar containing selenium server.
      server_timeout: server timeout value, in seconds.
    """
    self.selenium_alive = threading.Event()
    self.selenium_port = 0
    self.verbose = verbose
    self.java_path = java_path
    self.selenium_server = selenium_server
    self.timeout = server_timeout
    threading.Thread.__init__(self)

  def run(self):
    """Starts the selenium server.

    Server is started on an available port in the range of
    START_PORT to END_PORT
    """

    for self.selenium_port in range(self.START_PORT, self.END_PORT):
      # Attempt to start the selenium RC server from java
      self.process = subprocess.Popen(
          [self.java_path, "-jar", self.selenium_server, "-multiWindow",
           "-port", str(self.selenium_port), "-timeout", self.timeout],
          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

      for unused_i in range(1, 10):
        server_msg = self.process.stdout.readline()
        if self.verbose and server_msg is not None:
          # log if verbose flag is on
          print "sel_serv:" + server_msg

        # This status message indicates that the server has done
        # a bind to the port self.selenium_port successfully.
        if server_msg.find("INFO - Started SocketListener") != -1:
          self.selenium_alive.set()
          break

      # Error starting server on this port, try the next port.
      if not self.selenium_alive.isSet():
        continue

      # Loop and read from stdout
      while self.process.poll() is None:
        server_msg = self.process.stdout.readline()
        if self.verbose and server_msg is not None:
          # log if verbose flag is on
          print "sel_serv:" + server_msg

      # Finish.
      break

    if not self.selenium_alive.isSet():
      print("No available port found for Selenium RC Server "
            "in the range %d to %d."
            % (self.START_PORT, self.END_PORT))

  @staticmethod
  def StartServer(verbose, java_path, selenium_server, server_timeout):
    """Create and start the Selenium RC Server on a separate thread.

    Args:
      verbose: boolean verbose flag
      java_path: path to java used to run selenium.
      selenium_server: path to jar containing selenium server.
      server_timeout: server timeout value, in seconds

    Returns:
      selenium_server: SeleniumRemoteControl() object
    """

    # Start up the Selenium Remote Control server
    selenium_server = SeleniumRemoteControl(verbose,
                                            java_path,
                                            selenium_server,
                                            server_timeout)
    selenium_server.setDaemon(True)
    selenium_server.start()

    # Wait till the Selenium RC Server is up
    selenium_server.selenium_alive.wait()

    print("Selenium RC server started on port %d"
          % selenium_server.selenium_port)

    return selenium_server


class SeleniumSession(object):
  """A selenium browser session, with support servers.

    The support servers include a Selenium Remote Control server, and
    a local HTTP server to serve static test files.

  Members:
    session: a selenium() instance
    selenium_server: a SeleniumRemoteControl() instance
    http_server: a LocalFileHTTPServer() instance
    runner: a TestRunner() instance
  """

  def __init__(self, verbose, java_path, selenium_server, server_timeout):
    """Initializes a Selenium Session.

    Args:
      verbose: boolean verbose flag
      java_path: path to java used to run selenium.
      selenium_server: path to jar containing selenium server.
      server_timeout: server timeout value, in seconds
    """
    # Start up a static file server, to serve the test pages.
    # Serve from the o3d directory
    self.http_server = LocalFileHTTPServer.StartServer(
        TESTING_ROOT + "/../")

    # Start up the Selenium Remote Control Server
    self.selenium_server = SeleniumRemoteControl.StartServer(verbose,
                                                             java_path,
                                                             selenium_server,
                                                             server_timeout)

    # Set up a testing runner
    self.runner = pulse_testrunner.PulseTestRunner()

    # Set up a phantom selenium session so we can call shutdown if needed.
    self.session = selenium.selenium(
        "localhost", self.selenium_server.selenium_port, "*firefox",
        "http://" + socket.gethostname() + ":" +
        str(self.http_server.http_port))

  def StartSession(self, browser):
    """Starts the Selenium Session and connects to the HTTP server.

    Args:
      browser: selenium browser name
    """

    if browser == "*googlechrome":
      # TODO: Replace socket.gethostname() with "localhost"
      #                 once Chrome local proxy fix is in.
      server_url = "http://" + socket.gethostname() + ":"
    else:
      server_url = "http://localhost:"
    server_url += str(self.http_server.http_port)
    browser_path_with_space = FLAGS.browserpath

    # TODO: This is a hack to figure out if we're on 64-bit
    # Windows.  If we are, then we have to run the 32-bit Internet
    # Explorer so that our plugin will work (indeed, even Microsoft
    # has even made it impossible to use 64-bit Internet Explorer as
    # your default browser).  We need to find a better way to
    # determine if we're on 64-bit Windows, so that it will work on
    # foreign machines (which don't use the strings below for "Program
    # Files" and "Internet Explorer").
    if (not browser_path_with_space and
        browser == "*iexplore"):
      program_files_x86 = "C:\\Program Files (x86)"
      if os.path.isdir(program_files_x86):
        browser_path_with_space = os.path.join(program_files_x86,
                                               "Internet Explorer",
                                               "iexplore.exe")

    if browser_path_with_space:
      browser_path_with_space = " " + browser_path_with_space
    self.session = selenium.selenium("localhost",
                                     self.selenium_server.selenium_port,
                                     browser + browser_path_with_space,
                                     server_url)
    self.session.start()

  def CloseSession(self):
    """Closes the selenium sesssion."""
    self.session.stop()

  def TearDown(self):
    """Stops the selenium server."""
    self.session.do_command("shutDown", [])
    # Sync with selenium_server thread
    self.selenium_server.join()

  def TestBrowser(self, browser, test_list, test_prefix, test_suffixes,
                  server_timeout):
    """Runs Selenium tests for a specific browser.

    Args:
      browser: selenium browser name (eg. *iexplore, *firefox).
      test_list: list to add tests to.
      test_prefix: prefix of tests to run.
      test_suffixes: comma separated suffixes of tests to run.
      server_timeout: server timeout value, in milliseconds

    Returns:
      result: result of test runner.
    """
    print "Testing %s..." % browser
    self.StartSession(browser)
    self.session.set_timeout(server_timeout)
    self.runner.setBrowser(browser)

    try:
      result = self.runner.run(
          SeleniumSuite(self.session, browser, test_list,
                        test_prefix, test_suffixes))
    finally:
      self.CloseSession()

    return result


class LocalTestSuite(unittest.TestSuite):
  """Wrapper for unittest.TestSuite so we can collect the tests."""

  def __init__(self):
    unittest.TestSuite.__init__(self)
    self.test_list = []

  def addTest(self, name, test):
    """Adds a test to the TestSuite and records its name and test_path.

    Args:
      name: name of test.
      test: test to pass to unittest.TestSuite.
    """
    unittest.TestSuite.addTest(self, test)
    try:
      self.test_list.append((name, test.options))
    except AttributeError:
      self.test_list.append((name, []))


def MatchesSuffix(name, suffixes):
  """Checks if a name ends in one of the suffixes.

  Args:
    name: Name to test.
    suffixes: list of suffixes to test for.
  Returns:
    True if name ends in one of the suffixes or if suffixes is empty.
  """
  if suffixes:
    name_lower = name.lower()
    for suffix in suffixes:
      if name_lower.endswith(suffix):
        return True
    return False
  else:
    return True


def AddTests(test_suite, session, browser, module, filename, prefix,
             test_prefix_filter, test_suffixes):
  """Add tests defined in filename.

  Assumes module has a method "GenericTest" that uses self.args to run.

  Args:
    test_suite: A Selenium test_suite to add tests to.
    session: a Selenium instance.
    browser: browser name.
    module: module which will have method GenericTest() called to run each test.
    filename: filename of file with list of tests.
    prefix: prefix to add to the beginning of each test.
    test_prefix_filter: Only adds a test if it starts with this.
    test_suffixes: list of suffixes to filter by. An empty list = pass all.
  """
  # See comments in that file for the expected format.
  # skip lines that are blank or have "#" or ";" as their first non whitespace
  # character.
  test_list_file = open(filename, "r")
  samples = test_list_file.readlines()
  test_list_file.close()

  for sample in samples:
    sample = sample.strip()
    if not sample or sample[0] == ";" or sample[0] == "#":
      continue

    arguments = sample.split()
    test_type = arguments[0].lower()
    test_path = arguments[1]
    options = arguments[2:]

    # TODO: Add filter based on test_type

    name = ("Test" + prefix + re.sub("\W", "_", test_path) +
            test_type.capitalize())

    # Only execute this test if the current browser is not in the list
    # of skipped browsers.
    test_skipped = False
    for option in options:
      if option.startswith("except"):
        skipped_platforms = selenium_utilities.GetArgument(option)
        if not skipped_platforms is None:
          skipped_platforms = skipped_platforms.split(",")
          if browser in skipped_platforms:
            test_skipped = True

    if not test_skipped:
      # Check if there is already a test function by this name in the module.
      if (test_path.startswith(test_prefix_filter) and
          hasattr(module, test_path) and callable(getattr(module, test_path))):
        test_suite.addTest(test_path, module(test_path, session, browser,
                                             options=options))
      elif (name.startswith(test_prefix_filter) and
            MatchesSuffix(name, test_suffixes)):
        # no, so add a method that will run a test generically.
        setattr(module, name, module.GenericTest)
        test_suite.addTest(name, module(name, session, browser,
                                        test_type, test_path, options))


def SeleniumSuite(session, browser, test_list, test_prefix, test_suffixes):
  """Creates a test suite to run the unit tests.

  Args:
    session: a selenium() instance
    browser: browser name
    test_list: list to add tests to.
    test_prefix: prefix of tests to run.
    test_suffixes: A comma separated string of suffixes to filter by.
  Returns:
    A selenium test suite.
  """

  test_suite = LocalTestSuite()

  suffixes = test_suffixes.split(",")

  # add sample tests.
  filename = os.path.join(os.getcwd(), "tests", "selenium", "sample_list.txt")
  AddTests(test_suite,
           session,
           browser,
           samples_tests.SampleTests,
           filename,
           "Sample",
           test_prefix,
           suffixes)

  # add javascript tests.
  filename = os.path.join(os.getcwd(), "tests", "selenium",
                          "javascript_unit_test_list.txt")
  AddTests(test_suite,
           session,
           browser,
           javascript_unit_tests.JavaScriptUnitTests,
           filename,
           "UnitTest",
           test_prefix,
           suffixes)

  test_list += test_suite.test_list

  return test_suite


def CompareScreenshots(browser, test_list, screencompare, screenshotsdir,
                       screencompare_tool, verbose):
  """Performs the image validation for test-case frame captures.

  Args:
    browser: selenium browser name
    test_list: list of tests that ran.
    screencompare: True to actually run tests.
    screenshotsdir: path of directory containing images to compare.
    screencompare_tool: path to image diff tool.
    verbose: If True then outputs verbose info.

  Returns:
    A Results object.
  """
  print "Validating captured frames against reference data..."

  class Results(object):
    """An object to return results of screenshot compares.

    Similar to unittest.TestResults.
    """

    def __init__(self):
      object.__init__(self)
      self.tests_run = 0
      self.current_test = None
      self.errors = []
      self.failures = []
      self.start_time = 0

    def StartTest(self, test):
      """Adds a test.

      Args:
        test: name of test.
      """
      self.start_time = time.time()
      self.tests_run += 1
      self.current_test = test

    def TimeTaken(self):
      """Returns the time since the last call to StartTest."""
      return time.time() - self.start_time

    def AddFailure(self, test, browser, message):
      """Adds a failure.

      Args:
        test: name of the test.
        browser: name of the browser.
        message: error message to print
      """
      self.failures.append(test)
      print "ERROR: ", message
      print("SELENIUMRESULT %s <%s> [%.3fs]: FAIL"
            % (test, browser, self.TimeTaken()))

    def AddSuccess(self, test):
      """Adds a success.

      Args:
        test: name of the test.
      """
      print("SELENIUMRESULT %s <%s> [%.3fs]: PASS"
            % (test, browser, self.TimeTaken()))

    def WasSuccessful(self):
      """Returns true if all tests were successful."""
      return not self.errors and not self.failures

  results = Results()

  if not screencompare:
    return results

  base_path = os.getcwd()

  reference_files = os.listdir(os.path.join(
      base_path,
      selenium_constants.REFERENCE_SCREENSHOT_PATH))

  generated_files = os.listdir(os.path.join(base_path, screenshotsdir))

  # Prep the test list for matching
  temp = []
  for (test, options) in test_list:
    test = selenium_utilities.StripTestTypeSuffix(test)
    temp.append((test.lower(), options))
  test_list = temp

  # Create regex object for filename
  # file is in format "FILENAME_reference.png"
  reference_file_name_regex = re.compile(r"^(.*)_reference\.png")
  generated_file_name_regex = re.compile(r"^(.*)\.png")

  # check that there is a reference file for each generated file.
  for file_name in generated_files:
    match = generated_file_name_regex.search(file_name)

    if match is None:
      # no matches
      continue

    # Generated file name without png extension
    actual_name = match.group(1)

    # Get full paths to reference and generated files
    reference_file = os.path.join(
        base_path,
        selenium_constants.REFERENCE_SCREENSHOT_PATH,
        actual_name + "_reference.png")
    generated_file = os.path.join(
        base_path,
        screenshotsdir,
        actual_name + ".png")

    test_name = "TestReferenceScreenshotExists_" + actual_name
    results.StartTest(test_name)
    if not os.path.exists(reference_file):
      # reference file does not exist
      results.AddFailure(
          test_name, browser,
          "Missing reference file %s for generated file %s." %
          (reference_file, generated_file))
    else:
      results.AddSuccess(test_name)

  # Assuming both the result and reference image sets are the same size,
  # verify that corresponding images are similar within tolerance.
  for file_name in reference_files:
    match = reference_file_name_regex.search(file_name)

    if match is None:
      # no matches
      continue

    # Generated file name without png extension
    actual_name = match.group(1)
    # Get full paths to reference and generated files
    reference_file = os.path.join(
        base_path,
        selenium_constants.REFERENCE_SCREENSHOT_PATH,
        file_name)
    platform_specific_reference_file = os.path.join(
        base_path,
        selenium_constants.PLATFORM_SPECIFIC_REFERENCE_SCREENSHOT_PATH,
        actual_name + "_reference.png")
    generated_file = os.path.join(
        base_path,
        screenshotsdir,
        actual_name + ".png")

    # Generate a test case name
    test_name = "TestScreenCompare_" + actual_name

    # skip the reference file if the test is not in the test list.
    basename = os.path.splitext(os.path.basename(file_name))[0]
    basename = re.sub("\d+_reference", "", basename).lower()
    basename = re.sub("\W", "_", basename)
    test_was_run = False
    test_options = []
    for (test, options) in test_list:
      if test.endswith(basename):
        test_was_run = True
        test_options = options or []
        break

    if test_was_run:
      results.StartTest(test_name)
    else:
      # test was not planned to run for this reference image.
      if os.path.exists(generated_file):
        # a generated file exists? The test name does not match the screenshot.
        results.StartTest(test_name)
        results.AddFailure(test_name, browser,
                           "Test name and screenshot name do not match.")
      continue

    # Check if there is a platform specific version of the reference image
    if os.path.exists(platform_specific_reference_file):
      reference_file = platform_specific_reference_file

    # Check if perceptual diff exists
    pdiff_path = os.path.join(base_path, screencompare_tool)
    if not os.path.exists(pdiff_path):
      # Perceptualdiff.exe does not exist, fail.
      results.AddFailure(
          test_name, browser,
          "Perceptual diff %s does not exist." % pdiff_path)
      continue

    pixel_threshold = "10"
    use_colorfactor = False
    use_downsample = False

    # Find out if the test specified any options relating to perceptual diff
    # that will override the defaults.
    for opt in test_options:
      if opt.startswith("pdiff_threshold"):
        pixel_threshold = selenium_utilities.GetArgument(opt)
      elif (opt.startswith("pdiff_threshold_mac") and
            platform.system() == "Darwin"):
        pixel_threshold = selenium_utilities.GetArgument(opt)
      elif (opt.startswith("pdiff_threshold_win") and
            platform.system() == "Microsoft"):
        pixel_threshold = selenium_utilities.GetArgument(opt)
      elif (opt.startswith("pdiff_threshold_linux") and
            platform.system() == "Linux"):
        pixel_threshold = selenium_utilities.GetArgument(opt)
      elif (opt.startswith("colorfactor")):
        colorfactor = selenium_utilities.GetArgument(opt)
        use_colorfactor = True
      elif (opt.startswith("downsample")):
        downsample_factor = selenium_utilities.GetArgument(opt)
        use_downsample = True

    # Check if file exists
    if os.path.exists(generated_file):
      diff_file = os.path.join(base_path, screenshotsdir,
                               "compare_%s.png" % actual_name)

      # Run perceptual diff
      arguments = [pdiff_path,
                   reference_file,
                   generated_file,
                   "-output", diff_file,
                   "-fov", "45",
                   # Turn on verbose output for the percetual diff so we
                   # can see how far off we are on the threshold.
                   "-verbose",
                   # Set the threshold to zero so we can get a count
                   # of the different pixels.  This causes the program
                   # to return failure for most images, but we can compare
                   # the values ourselves below.
                   "-threshold", "0"]
      if use_colorfactor:
        arguments += ["-colorfactor", colorfactor]
      if use_downsample:
        arguments += ["-downsample", downsample_factor]

      # Print the perceptual diff command line so we can debug easier.
      if verbose:
        print " ".join(arguments)

      # diff tool should return 0 on success
      expected_result = 0

      pdiff_pipe = subprocess.Popen(arguments,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
      (pdiff_stdout, pdiff_stderr) = pdiff_pipe.communicate()
      result = pdiff_pipe.returncode

      # Find out how many pixels were different by looking at the output.
      pixel_re = re.compile("(\d+) pixels are different", re.DOTALL)
      pixel_match = pixel_re.search(pdiff_stdout)
      different_pixels = "0"
      if pixel_match:
        different_pixels = pixel_match.group(1)

      if (result == expected_result or (pixel_match and
          int(different_pixels) <= int(pixel_threshold))):
        # The perceptual diff passed.
        pass_re = re.compile("PASS: (.*?)\n", re.DOTALL)
        pass_match = pass_re.search(pdiff_stdout)
        reason = "Images are not perceptually different."
        if pass_match:
          reason = pass_match.group(1)
        print ("%s PASSED with %s different pixels "
               "(threshold %s) because: %s" % (test_name,
                                               different_pixels,
                                               pixel_threshold,
                                               reason))
        results.AddSuccess(test_name)
      else:
        # The perceptual diff failed.
        if pixel_match and int(different_pixels) > int(pixel_threshold):
          results.AddFailure(
              test_name, browser,
              ("Reference framebuffer (%s) does not match generated "
               "file (%s): it differed by %s pixels with a threshold of %s." %
               (reference_file, generated_file, different_pixels,
                pixel_threshold)))
        else:
          # The perceptual diff failed for some reason other than
          # pixel differencing.
          fail_re = re.compile("FAIL: (.*?)\n", re.DOTALL)
          fail_match = fail_re.search(pdiff_stdout)
          reason = "Unknown failure"
          if fail_match:
            reason = fail_match.group(1)
          results.AddFailure(
              test_name, browser,
              ("Perceptual diff of reference (%s) and generated (%s) files "
               "failed because: %s" %
               (reference_file, generated_file, reason)))
    else:
      # Generated file does not exist
      results.AddFailure(test_name, browser,
                         "File %s does not exist." % generated_file)

  return results


def main(unused_argv):
  # Boolean to record if all tests passed.
  all_tests_passed = True

  selenium_constants.REFERENCE_SCREENSHOT_PATH = os.path.join(
    FLAGS.referencedir,
    "reference",
    "")
  selenium_constants.PLATFORM_SPECIFIC_REFERENCE_SCREENSHOT_PATH = os.path.join(
    FLAGS.referencedir,
    selenium_constants.PLATFORM_SCREENSHOT_DIR,
    "")

  # Open a new session to Selenium Remote Control
  selenium_session = SeleniumSession(FLAGS.verbose, FLAGS.java,
                                     FLAGS.selenium_server,
                                     FLAGS.servertimeout)
  for browser in FLAGS.browser:
    if browser in set(selenium_constants.SELENIUM_BROWSER_SET):
      test_list = []
      result = selenium_session.TestBrowser(browser, test_list,
                                            FLAGS.testprefix,
                                            FLAGS.testsuffixes,
                                            int(FLAGS.servertimeout) * 1000)

      # Compare screenshots
      compare_result = CompareScreenshots(browser,
                                          test_list,
                                          FLAGS.screenshots,
                                          FLAGS.screenshotsdir,
                                          FLAGS.screencompare,
                                          FLAGS.verbose)
      if not result.wasSuccessful() or not compare_result.WasSuccessful():
        all_tests_passed = False
      # Log results
      print "Results for %s:" % browser
      print "  %d tests run." % (result.testsRun + compare_result.tests_run)
      print "  %d errors." % (len(result.errors) + len(compare_result.errors))
      print "  %d failures.\n" % (len(result.failures) +
                                  len(compare_result.failures))

    else:
      print "ERROR: Browser %s is invalid." % browser
      print "Run with --help to view list of supported browsers.\n"
      all_tests_passed = False

  # Wrap up session
  selenium_session.TearDown()

  if all_tests_passed:
    # All tests successful.
    return 0
  else:
    # Return error code 1.
    return 1

if __name__ == "__main__":
  remaining_argv = FLAGS(sys.argv)
  main(remaining_argv)
