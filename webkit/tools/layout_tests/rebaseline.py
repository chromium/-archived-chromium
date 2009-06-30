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

At the end, the script generates a html that compares old and new baselines.
"""

import logging
import optparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib
import webbrowser
import zipfile

from layout_package import path_utils
from layout_package import platform_utils_linux
from layout_package import platform_utils_mac
from layout_package import platform_utils_win
from layout_package import test_expectations

BASELINE_SUFFIXES = ['.txt', '.png', '.checksum']


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
                       stderr=subprocess.STDOUT, shell=use_shell)
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

  msg = text
  if platform:
    msg += ': ' + platform
  if len(msg) < 78:
    dashes = '-' * ((78 - len(msg)) / 2)
    msg = '%s %s %s' % (dashes, msg, dashes)

  if logging_level == logging.ERROR:
    logging.error(msg)
  elif logging_level == logging.WARNING:
    logging.warn(msg)
  else:
    logging.info(msg)


def SetupHtmlDirectory(html_directory, clean_html_directory):
  """Setup the directory to store html results.

     All html related files are stored in the "rebaseline_html" subdirectory.

  Args:
    html_directory: parent directory that stores the rebaselining results.
                    If None, a temp directory is created.
    clean_html_directory: if True, all existing files in the html directory
                          are removed before rebaselining.

  Returns:
    the directory that stores the html related rebaselining results.
  """

  if not html_directory:
    html_directory = tempfile.mkdtemp()
  elif not os.path.exists(html_directory):
    os.mkdir(html_directory)

  html_directory = os.path.join(html_directory, 'rebaseline_html')
  logging.info('Html directory: "%s"', html_directory)

  if clean_html_directory and os.path.exists(html_directory):
    shutil.rmtree(html_directory, True)
    logging.info('Deleted file at html directory: "%s"', html_directory)

  if not os.path.exists(html_directory):
    os.mkdir(html_directory)
  return html_directory


def GetResultFileFullpath(html_directory, baseline_filename, platform,
                          result_type):
  """Get full path of the baseline result file.

  Args:
    html_directory: directory that stores the html related files.
    baseline_filename: name of the baseline file.
    platform: win, linux or mac
    result_type: type of the baseline result: '.txt', '.png'.

  Returns:
    Full path of the baseline file for rebaselining result comparison.
  """

  base, ext = os.path.splitext(baseline_filename)
  result_filename = '%s-%s-%s%s' % (base, platform, result_type, ext)
  fullpath = os.path.join(html_directory, result_filename)
  logging.debug('  Result file full path: "%s".', fullpath)
  return fullpath


class Rebaseliner(object):
  """Class to produce new baselines for a given platform."""

  REVISION_REGEX = r'<a href=\"(\d+)/\">'

  def __init__(self, platform, options):
    self._file_dir = path_utils.GetAbsolutePath(os.path.dirname(sys.argv[0]))
    self._platform = platform
    self._options = options
    self._rebaselining_tests = []
    self._rebaselined_tests = []

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
    if not self._CompileRebaseliningTests():
      return True

    LogDashedString('Downloading archive', self._platform)
    archive_file = self._DownloadBuildBotArchive()
    logging.info('')
    if not archive_file:
      logging.error('No archive found.')
      return False

    LogDashedString('Extracting and adding new baselines', self._platform)
    if not self._ExtractAndAddNewBaselines(archive_file):
      return False

    LogDashedString('Updating rebaselined tests in file', self._platform)
    self._UpdateRebaselinedTestsInFile(backup)
    logging.info('')

    if len(self._rebaselining_tests) != len(self._rebaselined_tests):
      logging.warning('NOT ALL TESTS THAT NEED REBASELINING HAVE BEEN '
                      'REBASELINED.')
      logging.warning('  Total tests needing rebaselining: %d',
                      len(self._rebaselining_tests))
      logging.warning('  Total tests rebaselined: %d',
                      len(self._rebaselined_tests))
      return False

    logging.warning('All tests needing rebaselining were successfully '
                    'rebaselined.')

    return True

  def GetRebaseliningTests(self):
    return self._rebaselining_tests

  def _CompileRebaseliningTests(self):
    """Compile list of tests that need rebaselining for the platform.

    Returns:
      List of tests that need rebaselining or
      None if there is no such test.
    """

    self._rebaselining_tests = self._test_expectations.GetRebaseliningFailures()
    if not self._rebaselining_tests:
      logging.warn('No tests found that need rebaselining.')
      return None

    logging.info('Total number of tests needing rebaselining for "%s": "%d"',
                 self._platform, len(self._rebaselining_tests))

    test_no = 1
    for test in self._rebaselining_tests:
      logging.info('  %d: %s', test_no, test)
      test_no += 1

    return self._rebaselining_tests

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

  def _ExtractAndAddNewBaselines(self, archive_file):
    """Extract new baselines from archive and add them to SVN repository.

    Args:
      archive_file: full path to the archive file.

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
    self._rebaselined_tests = []
    for test in self._rebaselining_tests:
      logging.info('Test %d: %s', test_no, test)

      found = False
      svn_error = False
      test_basename = os.path.splitext(test)[0]
      for suffix in BASELINE_SUFFIXES:
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
        expected_fullpath = os.path.normpath(expected_fullpath)
        logging.debug('  Expected file full path: "%s"', expected_fullpath)

        data = zip_file.read(archive_test_name)
        f = open(expected_fullpath, 'wb')
        f.write(data)
        f.close()

        if not self._SvnAdd(expected_fullpath):
          svn_error = True
        elif suffix != '.checksum':
          self._CreateHtmlBaselineFiles(expected_fullpath)

      if not found:
        logging.warn('  No new baselines found in archive.')
      else:
        if svn_error:
          logging.warn('  Failed to add baselines to SVN.')
        else:
          logging.info('  Rebaseline succeeded.')
          self._rebaselined_tests.append(test)

      test_no += 1

    zip_file.close()
    os.remove(archive_file)

    return self._rebaselined_tests

  def _UpdateRebaselinedTestsInFile(self, backup):
    """Update the rebaselined tests in test expectations file.

    Args:
      backup: if True, backup the original test expectations file.

    Returns:
      no
    """

    if self._rebaselined_tests:
      self._test_expectations.RemovePlatformFromFile(self._rebaselined_tests,
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

    status_output = RunShell(['svn', 'status', filename], False)
    output = status_output.upper()
    if output.startswith('A') or output.startswith('M'):
      logging.info('  File already added to SVN: "%s"', filename)
      return True

    add_output = RunShell(['svn', 'add', filename], True)
    output = add_output.upper().rstrip()
    if output.startswith('A') and output.endswith(filename.upper()):
      logging.info('  Added new file: "%s"', filename)
      return True

    if (not status_output) and (add_output.upper().find(
        'ALREADY UNDER VERSION CONTROL')):
      logging.info('  File already under SVN and has no change: "%s"', filename)
      return True

    logging.warn('  Failed to add file to SVN: "%s"', filename)
    logging.warn('  Svn status output: "%s"', status_output)
    logging.warn('  Svn add output: "%s"', add_output)
    return False

  def _CreateHtmlBaselineFiles(self, baseline_fullpath):
    """Create baseline files (old, new and diff) in html directory.

       The files are used to compare the rebaselining results.

    Args:
      baseline_fullpath: full path of the expected baseline file.
    """

    if self._options.no_html_results:
      return

    if not baseline_fullpath or not os.path.exists(baseline_fullpath):
      return

    # Copy the new baseline to html directory for result comparison.
    baseline_filename = os.path.basename(baseline_fullpath)
    new_file = GetResultFileFullpath(self._options.html_directory,
                                     baseline_filename,
                                     self._platform,
                                     'new')
    shutil.copyfile(baseline_fullpath, new_file)
    logging.info('  Html: copied new baseline file from "%s" to "%s".',
                 baseline_fullpath, new_file)

    # Get the old baseline from SVN and save to the html directory.
    output = RunShell(['svn', 'cat', '-r', 'BASE', baseline_fullpath])
    if (not output) or (output.upper().rstrip().endswith(
        'NO SUCH FILE OR DIRECTORY')):
      logging.info('  No base file: "%s"', baseline_fullpath)
      return
    base_file = GetResultFileFullpath(self._options.html_directory,
                                      baseline_filename,
                                      self._platform,
                                      'old')
    f = open(base_file, 'wb')
    f.write(output)
    f.close()
    logging.info('  Html: created old baseline file: "%s".',
                 base_file)

    # Get the diff between old and new baselines and save to the html directory.
    if baseline_filename.upper().endswith('.TXT'):
      output = RunShell(['svn', 'diff', baseline_fullpath])
      if output:
        diff_file = GetResultFileFullpath(self._options.html_directory,
                                          baseline_filename,
                                          self._platform,
                                          'diff')
        f = open(diff_file, 'wb')
        f.write(output)
        f.close()
        logging.info('  Html: created baseline diff file: "%s".',
                     diff_file)


class HtmlGenerator(object):
  """Class to generate rebaselining result comparison html."""

  HTML_REBASELINE = ('<html>'
                     '<head>'
                     '<style>'
                     'body {font-family: sans-serif;}'
                     '.mainTable {background: #666666;}'
                     '.mainTable td , .mainTable th {background: white;}'
                     '.detail {margin-left: 10px; margin-top: 3px;}'
                     '</style>'
                     '<title>Rebaselining Result Comparison (%(time)s)</title>'
                     '</head>'
                     '<body>'
                     '<h2>Rebaselining Result Comparison (%(time)s)</h2>'
                     '%(body)s'
                     '</body>'
                     '</html>')
  HTML_NO_REBASELINING_TESTS = '<p>No tests found that need rebaselining.</p>'
  HTML_TABLE_TEST = ('<table class="mainTable" cellspacing=1 cellpadding=5>'
                     '%s</table><br>')
  HTML_TR_TEST = ('<tr>'
                  '<th style="background-color: #CDECDE; border-bottom: '
                  '1px solid black; font-size: 18pt; font-weight: bold" '
                  'colspan="5">'
                  '<a href="%s">%s</a>'
                  '</th>'
                  '</tr>')
  HTML_TEST_DETAIL = ('<div class="detail">'
                      '<tr>'
                      '<th width="100">Baseline</th>'
                      '<th width="100">Platform</th>'
                      '<th width="200">Old</th>'
                      '<th width="200">New</th>'
                      '<th width="150">Difference</th>'
                      '</tr>'
                      '%s'
                      '</div>')
  HTML_TD_NOLINK = '<td align=center><a>%s</a></td>'
  HTML_TD_LINK = '<td align=center><a href="%(uri)s">%(name)s</a></td>'
  HTML_TD_LINK_IMG = ('<td><a href="%(uri)s">'
                      '<img style="width: 200" src="%(uri)s" /></a></td>')
  HTML_TR = '<tr>%s</tr>'

  def __init__(self, options, platforms, rebaselining_tests):
    self._html_directory = options.html_directory
    self._browser_path = options.browser_path
    self._platforms = platforms
    self._rebaselining_tests = rebaselining_tests
    self._html_file = os.path.join(options.html_directory, 'rebaseline.html')

  def GenerateHtml(self):
    """Generate html file for rebaselining result comparison."""

    logging.info('Generating html file')

    html_body = ''
    if not self._rebaselining_tests:
      html_body += self.HTML_NO_REBASELINING_TESTS
    else:
      tests = list(self._rebaselining_tests)
      tests.sort()

      test_no = 1
      for test in tests:
        logging.info('Test %d: %s', test_no, test)
        html_body += self._GenerateHtmlForOneTest(test)

    html = self.HTML_REBASELINE % ({'time': time.asctime(), 'body': html_body})
    logging.debug(html)

    f = open(self._html_file, 'w')
    f.write(html)
    f.close()

    logging.info('Baseline comparison html generated at "%s"',
                 self._html_file)

  def ShowHtml(self):
    """Launch the rebaselining html in brwoser."""

    logging.info('Launching html: "%s"', self._html_file)

    html_uri = path_utils.FilenameToUri(self._html_file)
    if self._browser_path:
      RunShell([self._browser_path, html_uri], False)
    else:
      webbrowser.open(html_uri, 1)

    logging.info('Html launched.')

  def _GenerateBaselineLinks(self, test_basename, suffix, platform):
    """Generate links for baseline results (old, new and diff).

    Args:
      test_basename: base filename of the test
      suffix: baseline file suffixes: '.txt', '.png'
      platform: win, linux or mac

    Returns:
      html links for showing baseline results (old, new and diff)
    """

    baseline_filename = '%s-expected%s' % (test_basename, suffix)
    logging.debug('    baseline filename: "%s"', baseline_filename)

    new_file = GetResultFileFullpath(self._html_directory,
                                     baseline_filename,
                                     platform,
                                     'new')
    logging.info('    New baseline file: "%s"', new_file)
    if not os.path.exists(new_file):
      logging.info('    No new baseline file: "%s"', new_file)
      return ''

    old_file = GetResultFileFullpath(self._html_directory,
                                     baseline_filename,
                                     platform,
                                     'old')
    logging.info('    Old baseline file: "%s"', old_file)
    if suffix == '.png':
      html_td_link = self.HTML_TD_LINK_IMG
    else:
      html_td_link = self.HTML_TD_LINK

    links = ''
    if os.path.exists(old_file):
      links += html_td_link % {'uri': path_utils.FilenameToUri(old_file),
                               'name': baseline_filename}
    else:
      logging.info('    No old baseline file: "%s"', old_file)
      links += self.HTML_TD_NOLINK % ''

    links += html_td_link % {'uri': path_utils.FilenameToUri(new_file),
                             'name': baseline_filename}

    diff_file = GetResultFileFullpath(self._html_directory,
                                      baseline_filename,
                                      platform,
                                      'diff')
    logging.info('    Baseline diff file: "%s"', diff_file)
    if os.path.exists(diff_file):
      links += html_td_link % {'uri': path_utils.FilenameToUri(diff_file),
                               'name': 'Diff'}
    else:
      logging.info('    No baseline diff file: "%s"', diff_file)
      links += self.HTML_TD_NOLINK % ''

    return links

  def _GenerateHtmlForOneTest(self, test):
    """Generate html for one rebaselining test.

    Args:
      test: layout test name

    Returns:
      html that compares baseline results for the test.
    """

    test_basename = os.path.basename(os.path.splitext(test)[0])
    logging.info('  basename: "%s"', test_basename)
    rows = []
    for suffix in BASELINE_SUFFIXES:
      if suffix == '.checksum':
        continue

      logging.info('  Checking %s files', suffix)
      for platform in self._platforms:
        links = self._GenerateBaselineLinks(test_basename, suffix, platform)
        if links:
          row = self.HTML_TD_NOLINK % self._GetBaselineResultType(suffix)
          row += self.HTML_TD_NOLINK % platform
          row += links
          logging.debug('    html row: %s', row)

          rows.append(self.HTML_TR % row)

    if rows:
      test_path = os.path.join(path_utils.LayoutTestsDir(), test)
      html = self.HTML_TR_TEST % (path_utils.FilenameToUri(test_path), test)
      html += self.HTML_TEST_DETAIL % ' '.join(rows)

      logging.debug('    html for test: %s', html)
      return self.HTML_TABLE_TEST % html

    return ''

  def _GetBaselineResultType(self, suffix):
    """Name of the baseline result type."""

    if suffix == '.png':
      return 'Pixel'
    elif suffix == '.txt':
      return 'Render Tree'
    else:
      return 'Other'


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

  option_parser.add_option('-o', '--no_html_results',
                           action='store_true',
                           default=False,
                           help=('If specified, do not generate html that '
                                  'compares the rebaselining results.'))

  option_parser.add_option('-d', '--html_directory',
                           default='',
                           help=('The directory that stores the results for '
                                 'rebaselining comparison.'))

  option_parser.add_option('-c', '--clean_html_directory',
                           action='store_true',
                           default=False,
                           help=('If specified, delete all existing files in '
                                 'the html directory before rebaselining.'))

  option_parser.add_option('-e', '--browser_path',
                           default='',
                           help=('The browser path that you would like to '
                                 'use to launch the rebaselining result '
                                 'comparison html'))

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

  if not options.no_html_results:
    options.html_directory = SetupHtmlDirectory(options.html_directory,
                                                options.clean_html_directory)

  rebaselining_tests = set()
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

    rebaselining_tests |= set(rebaseliner.GetRebaseliningTests())

  if not options.no_html_results:
    logging.info('')
    LogDashedString('Rebaselining result comparison started', None)
    html_generator = HtmlGenerator(options, platforms, rebaselining_tests)
    html_generator.GenerateHtml()
    html_generator.ShowHtml()
    LogDashedString('Rebaselining result comparison done', None)

  sys.exit(0)

if '__main__' == __name__:
  main()
