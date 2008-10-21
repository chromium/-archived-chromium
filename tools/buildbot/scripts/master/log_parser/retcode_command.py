#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A buildbot command for running and interpreting tests with magic return
codes."""

from buildbot.process import buildstep
from buildbot.steps import shell
from buildbot.status import builder

import chromium_config as config


class ReturnCodeCommand(shell.ShellCommand):
  """Buildbot command that knows how to display layout test output."""

  # magic return code to indicate that the command ran with warnings and
  # the buildbot should turn orange.  See evaluteCommand.
  RETCODE_WARNINGS = config.Master.retcode_warnings

  def __init__(self, **kwargs):
    shell.ShellCommand.__init__(self, **kwargs)

  def evaluateCommand(self, cmd):
    """Decide whether the command was SUCCESS, WARNINGS, or FAILURE.
    A retcode of 0 is SUCCESS, 88 is WARNING, all else is FAILURE.
    Yes, -88 is an arbitrary value which may collide with something else, but
    with only 255 values and no way to reserve them, this is what we've got.
    """
    if cmd.rc == 0:
      return builder.SUCCESS
    if cmd.rc == ReturnCodeCommand.RETCODE_WARNINGS:
      return builder.WARNINGS
    return builder.FAILURE

  def getText(self, cmd, results):
    if results == builder.SUCCESS:
      return self.describe(True)
    elif results == builder.WARNINGS:
      return self.describe(True) + ['warning']
    else:
      return self.describe(True) + ['failed']
