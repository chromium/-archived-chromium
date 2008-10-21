#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to kill any leftover test processes, executed by buildbot.
"""

import os
import sys

import google.process_utils

# rdpclip.exe is part of Remote Desktop.  It has a bug that sometimes causes
# it to keep the clipboard open forever, denying other processes access to it.
# Killing BuildConsole.exe usually stops an IB build within a few seconds.
# Unfortunately, killing devenv.com or devenv.exe doesn't stop a VS build, so
# we don't bother pretending.
processes=[
    # Utilities we don't build, but which we use or otherwise can't
    # have hanging around.
    'BuildConsole.exe',
    'httpd.exe',
    'outlook.exe',
    'perl.exe',
    'purifye.exe',
    'purifyW.exe',
    'python_slave.exe',
    'rdpclip.exe',
    'svn.exe',

    # When VC crashes during compilation, this process which manages the .pdb
    # file generation sometime hangs.
    'mspdbsrv.exe',

    # Things built by/for Chromium.
    'base_unittests.exe',
    'chrome.exe',
    'crash_service.exe',
    'debug_message.exe',
    'flush_cache.exe',
    'image_diff.exe',
    'installer_unittests.exe',
    'interactive_ui_tests.exe',
    'ipc_tests.exe',
    'memory_test.exe',
    'net_unittests.exe',
    'page_cycler_tests.exe',
    'perf_tests.exe',
    'plugin_tests.exe',
    'reliability_tests.exe',
    'selenium_tests.exe',
    'startup_tests.exe',
    'tab_switching_test.exe',
    'test_shell.exe',
    'test_shell_tests.exe',
    'tld_cleanup.exe',
    'ui_tests.exe',
    'unit_tests.exe',
    'v8_shell.exe',
    'v8_mksnapshot.exe',
    'v8_shell_sample.exe',
    'wow_helper.exe',
]

if '__main__' == __name__:
  sys.exit(google.process_utils.KillAll(processes))
