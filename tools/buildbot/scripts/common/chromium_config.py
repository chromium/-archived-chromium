#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Declares a number of site-dependent variables for use by scripts.

A typical use of this module would be

  import chromium_config as config

  svn_path = config.Master.svn_binary_path
"""

import errno
import os


class Master(object):
  """Buildbot master configuration options."""

  # Which port slaves use to connect to the master.
  slave_port = 8012
  slave_port_submodules = 8013

  # Used by the SVNPoller.
  svn_binary_path = 'svn'
  svn_binary_path_experimental = '../depot_tools/bin/svn'

  # Repository URLs used by the SVNPoller and 'gclient config'.
  server_url = 'http://src.chromium.org'
  trunk_url = server_url + '/svn/trunk'
  trunk_internal_url = None
  branch_url = server_url + '/svn/branches'
  merge_branch_url = branch_url + '/chrome_webkit_merge_branch'
  v8_url = 'http://v8.googlecode.com/svn/branches/bleeding_edge'

  # Default target platform if none was given to the factory.
  default_platform = 'win32'

  # Used for the waterfall URL and as the From: address when buildbot is
  # configured to send email notifications.
  master_host = 'localhost'
  master_host_experimental = 'localhost'

  # Used for the waterfall URL and the waterfall's WebStatus object.
  master_domain = ''
  master_port = 8010
  master_port_submodules = 8011

  # Used by the waterfall display.
  project_name = 'Chromium'
  project_name_submodules = 'Chromium (submodules)'
  project_url = 'http://www.chromium.org'
  buildbot_url = 'http://%s.%s:%s' % (master_host, master_domain, master_port)
  buildbot_url_submodules = 'http://%s.%s:%s' % (master_host, master_domain,
                                                 master_port_submodules)

  # Where the IRC bot lives.
  irc_host = 'irc.freenode.net'
  irc_channels = ['#chromium']
  irc_nickname = 'cr_buildbot'
  irc_nickname_submodules = 'buildbot_sub'

  # Configuration for email notifications, if they're enabled.
  notifications_from = 'buildbot@%s' % master_domain

  # Base URL for perf test results.
  perf_base_url = 'http://build.chromium.org/buildbot/perf'

  # Suffix for perf URL.
  perf_report_url_suffix = 'report.html?history=150'

  # Directory in which to save perf-test output data files.
  perf_output_dir = '~/www/perf'

  # URL pointing to builds and test results.
  archive_url = 'http://build.chromium.org/buildbot'

  # File in which to save a list of graph names.
  perf_graph_list = 'graphs.dat'

  # Magic step return code inidicating "warning(s)" rather than "error".
  retcode_warnings = -88

  bot_password = 'change_me'

  @staticmethod
  def GetBotPassword():
    return Master.bot_password


class Installer(object):
  """Installer configuration options."""

  # Executable name.
  installer_exe = 'mini_installer.exe'

  # A file containing information about the last release.
  last_release_info = "."

  # Section in that file containing applicable values.
  file_section = 'CHROME'

  # File holding current version information.
  version_file = 'VERSION'

  # Output of mini_installer project.
  output_file = 'packed_files.txt'


class Archive(object):
  """Build and data archival options."""

  # List of symbol files archived by official and dev builds.
  # It's sad to have to hard-code these here.
  symbols_to_archive = ['chrome_dll.pdb', 'chrome_exe.pdb',
                        'mini_installer.pdb', 'setup.pdb', 'rlz_dll.pdb']

  # Binary to archive on the source server with the sourcified symbols.
  symsrc_binaries = ['chrome.exe', 'chrome.dll']

  # List of symbol files to save, but not to upload to the symbol server
  # (generally because they have no symbols and thus would produce an error).
  symbols_to_skip_upload = ['icudt38.dll']

  # Skip any filenames (exes, symbols, etc.) starting with these strings
  # entirely, typically because they're not built for this distribution.
  exes_to_skip_entirely = ['rlz']

  # Extra files to archive in official mode.
  official_extras = [
    ['setup.exe'],
    ['chrome.packed.7z'],
    ['patch.packed.7z'], 
    ['obj', 'mini_installer', 'mini_installer_exe_version.rc'],
  ]

  # Installer to archive.
  installer_exe = Installer.installer_exe

  # Test files to archive.
  tests_to_archive = ['reliability_tests.exe',
                      'automated_ui_tests.exe',
                      'test_shell.exe',
                      'icudt38.dll',
                      'plugins\\npapi_layout_test_plugin.dll',
                     ]
  # Archive everything in these directories, using glob.
  test_dirs_to_archive = ['fonts']
  # Create these directories, initially empty, in the archive.
  test_dirs_to_create = ['plugins', 'fonts']

  # URLs to pass to breakpad/symupload.exe.
  symbol_url = 'not available'
  symbol_staging_url = 'not available' 

  # Web server base path.
  www_dir_base = "\\\\localhost\\www\\"

  # Directories in which to store built files, for dev, official, and full
  # builds. (We don't use the full ones yet.)
  www_dir_base_dev = www_dir_base + 'snapshots'
  www_dir_base_official = www_dir_base + 'official_builds'
  www_dir_base_full = 'unused'
  symbol_dir_base_dev = www_dir_base_dev
  symbol_dir_base_full = www_dir_base_full
  symbol_dir_base_official = www_dir_base_official

  # Where to find layout test results by default, above the build directory.
  layout_test_result_dir = 'layout-test-results'

  # Where to save layout test results.
  layout_test_result_archive = www_dir_base + 'layout_test_results'


class IRC(object):
  """Options for the IRC bot."""

  default_topic = 'IRC bot not yet connected'

  whuffie_file = '~/www/irc/whuffie_list.js'
  whuffie_reason_file = '~/www/irc/whuffie_reasons.js'
  topic_file = '~/www/irc/topic_list.js'

  bot_admins = ['root']

  # Any URLs found in IRC topics will be passed as %s to this format before
  # being added to the topic-list page. It must contain exactly one "%s" token.
  # To disable URL mangling, set this to "%s".
  href_redirect_format = 'http://www.google.com/url?sa=D&q=%s'


try:
  # Define anything you want in the chromium_config_private file.
  import chromium_config_private
  # You can also override the properties defined above in this function.
  chromium_config_private.LoadPrivateConfiguration()
except ImportError:
  pass
