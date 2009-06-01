#!/bin/env python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rebaselining tool that automatically produces baselines for all platforms.

The script does the following for each platform specified:
  1. Compile a list of tests that need rebaselining.
  2. Download test result archive from buildbot for the platform.
  3. Extract baselines from the archive file for all identified files.
  4. Add new baselines to SVN repository.
  5. For each test that has been rebaselined, remove this platform option from
     the test in test_expectation.txt. If no other platforms remain after
     removal, delete the rebaselined test from the file.
"""

import logging
import optparse
import os
import re
import subprocess
import sys
import urllib
import zipfile

from layout_package import path_utils
from layout_package import platform_utils_linux
from layout_package import platform_utils_mac
from layout_package import platform_utils_win
from layout_package import test_expectations


def RunShell(command, print_output=False):
  """Executes a command and returns the output.

  Args:
    command: program and arguments.
    print_output: if true, print the command results to standard output.

  Returns:
    command output
  """

  # Use a shell for subcommands on Windows to get a PATH search.
  use_shell = sys.platform.startswith('win')
  p = subprocess.Popen(command, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, shell=use_shell,
                       universal_newlines=True)
  if print_output:
    output_array = []
    while True:
      line = p.stdout.readline()
      if not line:
        break
      if print_output:
        print line.strip('\n')
      output_array.append(line)
    output = ''.join(output_array)
  else:
    output = p.stdout.read()
  p.wait()
  p.stdout.close()
  return output


def LogDashedString(text, platform, logging_level=logging.INFO):
  """Log text message with dashes on both sides."""

  msg = '%s: %s' % (text, platform)
  if len(msg) < 78:
    dashes = '-' * ((78 - len(msg)) / 2)
    msg = '%s %s %s' % (dashes, msg, dashes)

  if logging_level == logging.ERROR:
    logging.error(msg)
  elif logging_level == logging.WARNING:
    logging.warn(msg)
  else:
    logging.info(msg)


class Rebaseliner(object):
  """Class to produce new baselines for a given platform."""

  BASELINE_SUFFIXES = ['.txt', '.png', '.checksum']
  REVISION_REGEX = r'<a href=\"(\d+)/\">'

  def __init__(self, platform, options):
    self._file_dir = path_utils.GetAbsolutePath(os.path.dirname(sys.argv[0]))
    self._platform = platform
    self._options = options

    # Create tests and expectations helper which is used to:
    #   -. compile list of tests that need rebaselining.
    #   -. update the tests in test_expectations file after rebaseline is done.
    self._test_expectations = test_expectations.TestExpectations(None,
                                                                 self._file_dir,
                                                                 platform,
                                                                 False)

  def Run(self, backup):
    """Run rebaseline process."""

    LogDashedString('Compiling rebaselining tests', self._platform)
    rebaselining_tests = self._CompileRebaseliningTests()
    logging.info('')
    if rebaselining_tests is None:
      return True

    LogDashedString('Downloading archive', self._platform)
    archive_file = self._DownloadBuildBotArchive()
    logging.info('')
    if not archive_file:
      logging.error('No archive found.')
      return False

    rebaselined_tests = []
    LogDashedString('Extracting and adding new baselines', self._platform)
    rebaselined_tests = self._ExtractAndAddNewBaselines(archive_file,
                                                        rebaselining_tests)
    logging.info('')

    LogDashedString('Updating rebaselined tests in file', self._platform)
    self._UpdateRebaselinedTestsInFile(rebaselined_tests, backup)
    logging.info('')

    if len(rebaselining_tests) != len(rebaselined_tests):
      logging.warning('NOT ALL TESTS THAT NEED REBASELINING HAVE BEEN '
                      'REBASELINED.')
      logging.warning('  Total tests needing rebaselining: %d',
                      len(rebaselining_tests))
      logging.warning('  Total tests rebaselined: %d',
                      len(rebaselined_tests))
      return False

    logging.warning('All tests needing rebaselining were successfully '
                    'rebaselined.')

    return True

  def _CompileRebaseliningTests(self):
    """Compile list of tests that need rebaselining for the platform.

    Returns:
      List of tests that need rebaselining or
      None if there is no such test.
    """

    rebaselining_tests = self._test_expectations.GetRebaseliningFailures()
    if not rebaselining_tests:
      logging.warn('No tests found that need rebaselining.')
      return None

    logging.info('Total number of tests needing rebaselining for "%s": "%d"',
                 self._platform, len(rebaselining_tests))

    test_no = 1
    for test in rebaselining_tests:
      logging.info('  %d: %s', test_no, test)
      test_no += 1

    return rebaselining_tests

  def _GetLatestRevision(self, url):
    """Get the latest layout test revision number from buildbot.

    Args:
      url: Url to retrieve layout test revision numbers.

    Returns:
      latest revision or
      None on failure.
    """

    logging.debug('Url to retrieve revision: "%s"', url)

    f = urllib.urlopen(url)
    content = f.read()
    f.close()

    revisions = re.findall(self.REVISION_REGEX, content)
    if not revisions:
      logging.error('Failed to find revision, content: "%s"', content)
      return None

    revisions.sort(key=int)
    logging.info('Latest revision: "%s"', revisions[len(revisions) - 1])
    return revisions[len(revisions) - 1]

  def _GetArchiveUrl(self):
    """Generate the url to download latest layout test archive.

    Returns:
      Url to download archive or
      None on failure
    """

    platform_name = self._options.buildbot_platform_dir_basename
    if not self._platform == 'win':
      platform_name += '-' + self._platform
    logging.debug('Buildbot platform dir name: "%s"', platform_name)

    url_base = '%s/%s/' % (self._options.archive_url, platform_name)
    latest_revision = self._GetLatestRevision(url_base)
    if latest_revision is None or latest_revision <= 0:
      return None

    archive_url = ('%s%s/%s.zip' % (url_base,
                                    latest_revision,
                                    self._options.archive_name))
    logging.info('Archive url: "%s"', archive_url)
    return archive_url

  def _DownloadBuildBotArchive(self):
    """Download layout test archive file from buildbot.

    Returns:
      True if download succeeded or
      False otherwise.
    """

    url = self._GetArchiveUrl()
    if url is None:
      return None

    fn = urllib.urlretrieve(url)[0]
    logging.info('Archive downloaded and saved to file: "%s"', fn)
    return fn

  def _GetPlatformNewResultsDir(self):
    """Get the dir name to extract new baselines for the given platform."""

    if self._platform == 'win':
      return platform_utils_win.PlatformUtility(None).PlatformNewResultsDir()
    elif self._platform == 'mac':
      return platform_utils_mac.PlatformUtility(None).PlatformNewResultsDir()
    elif self._platform == 'linux':
      return platform_utils_linux.PlatformUtility(None).PlatformNewResultsDir()

    return None

  def _ExtractAndAddNewBaselines(self, archive_file, rebaselining_tests):
    """Extract new baselines from archive and add them to SVN repository.

    Args:
      archive_file: full path to the archive file.
      rebaselining_tests: list of tests that need rebaselining.

    Returns:
      List of tests that have been rebaselined or
      None on failure.
    """

    zip_file = zipfile.ZipFile(archive_file, 'r')
    zip_namelist = zip_file.namelist()

    logging.debug('zip file namelist:')
    for name in zip_namelist:
      logging.debug('  ' + name)

    platform_dir = self._GetPlatformNewResultsDir()
    if not platform_dir:
      logging.error('Invalid platform new results dir, platform: "%s"',
                    self._platform)
      return None

    logging.debug('Platform new results dir: "%s"', platform_dir)

    test_no = 1
    rebaselined_tests = []
    for test in rebaselining_tests:
      logging.info('Test %d: %s', test_no, test)

      found = False
      svn_error = False
      test_basename = os.path.splitext(test)[0]
      for suffix in self.BASELINE_SUFFIXES:
        archive_test_name = '%s/%s-actual%s' % (self._options.archive_name,
                                                test_basename,
                                                suffix)
        logging.debug('  Archive test file name: "%s"', archive_test_name)
        if not archive_test_name in zip_namelist:
          logging.info('  %s file not in archive.', suffix)
          continue

        found = True
        logging.info('  %s file found in archive.', suffix)

        expected_filename = '%s-expected%s' % (test_basename, suffix)
        expected_fullpath = os.path.join(
            path_utils.ChromiumPlatformResultsEnclosingDir(),
            platform_dir,
            expected_filename)
        expected_fullpath = os.path.normcase(expected_fullpath)
        logging.debug('  Expected file full path: "%s"', expected_fullpath)

        data = zip_file.read(archive_test_name)
        f = open(expected_fullpath, 'wb')
        f.write(data)
        f.close()

        if not self._SvnAdd(expected_fullpath):
          svn_error = True

      if not found:
        logging.warn('  No new baselines found in archive.')
      else:
        if svn_error:
          logging.warn('  Failed to add baselines to SVN.')
        else:
          logging.info('  Rebaseline succeeded.')
          rebaselined_tests.append(test)

      test_no += 1

    zip_file.close()
    os.remove(archive_file)

    return rebaselined_tests

  def _UpdateRebaselinedTestsInFile(self, rebaselined_tests, backup):
    """Update the rebaselined tests in test expectations file.

    Args:
      rebaselined_tests: list of tests that have been rebaselined.
      backup: if True, backup the original test expectations file.

    Returns:
      no
    """

    if rebaselined_tests:
      self._test_expectations.RemovePlatformFromFile(rebaselined_tests,
                                                     self._platform,
                                                     backup)
    else:
      logging.info('No test was rebaselined so nothing to remove.')

  def _SvnAdd(self, filename):
    """Add the file to SVN repository.

    Args:
      filename: full path of the file to add.

    Returns:
      True if the file already exists in SVN or is sucessfully added to SVN.
      False otherwise.
    """

    output = RunShell(['svn', 'status', filename], False)
    logging.debug('  Svn status output: "%s"', output)
    if output.startswith('A') or output.startswith('M'):
      logging.info('  File already added to SVN: "%s"', filename)
      return True

    output = RunShell(['svn', 'add', filename], True)
    logging.debug('  Svn add output: "%s"', output)
    if output.startswith('A') and output.endswith(filename):
      logging.info('  Added new file: "%s"', filename)
      return True

    logging.warn('  Failed to add file to SVN: "%s"', filename)
    return False


def main():
  """Main function to produce new baselines."""

  option_parser = optparse.OptionParser()
  option_parser.add_option('-v', '--verbose',
                           action='store_true',
                           default=False,
                           help='include debug-level logging.')

  option_parser.add_option('-p', '--platforms',
                           default='win,mac,linux',
                           help=('Comma delimited list of platforms that need '
                                 'rebaselining.'))

  option_parser.add_option('-u', '--archive_url',
                           default=('http://build.chromium.org/buildbot/'
                                    'layout_test_results'),
                           help=('Url to find the layout test result archive '
                                 'file.'))

  option_parser.add_option('-t', '--archive_name',
                           default='layout-test-results',
                           help='Layout test result archive name.')

  option_parser.add_option('-n', '--buildbot_platform_dir_basename',
                           default='webkit-rel',
                           help=('Base name of buildbot platform directory '
                                 'that stores the layout test results.'))

  option_parser.add_option('-b', '--backup',
                           action='store_true',
                           default=False,
                           help=('Whether or not to backup the original test '
                                 'expectations file after rebaseline.'))

  options = option_parser.parse_args()[0]

  # Set up our logging format.
  log_level = logging.INFO
  if options.verbose:
    log_level = logging.DEBUG
  logging.basicConfig(level=log_level,
                      format=('%(asctime)s %(filename)s:%(lineno)-3d '
                              '%(levelname)s %(message)s'),
                      datefmt='%y%m%d %H:%M:%S')

  # Verify 'platforms' option is valid
  if not options.platforms:
    logging.error('Invalid "platforms" option. --platforms must be specified '
                  'in order to rebaseline.')
    sys.exit(1)

  backup = options.backup
  platforms = [p.strip().lower() for p in options.platforms.split(',')]
  for platform in platforms:
    rebaseliner = Rebaseliner(platform, options)

    logging.info('')
    LogDashedString('Rebaseline started', platform)
    if rebaseliner.Run(backup):
      # Only need to backup one original copy of test expectation file.
      backup = False
      LogDashedString('Rebaseline done', platform)
    else:
      LogDashedString('Rebaseline failed', platform, logging.ERROR)

  sys.exit(0)

if '__main__' == __name__:
  main()
