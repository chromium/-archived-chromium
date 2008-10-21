#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A buildbot command for running and interpreting the build_archive.sh script.
"""

from buildbot.steps import shell
from buildbot.process import buildstep

class ScriptObserver(buildstep.LogLineObserver):
  """This class knows how to understand build_archive.sh test output."""

  def __init__(self):
    buildstep.LogLineObserver.__init__(self)
    self.last_change = ''

  def outLineReceived(self, line):
    """This is called once with each line of the test log."""
    if line.startswith('last change: '):
      self.last_change = line.split(' ')[2]

class ArchiveCommand(shell.ShellCommand):
  """Buildbot command that knows how to display build_archive.sh output."""

  def __init__(self, **kwargs):
    shell.ShellCommand.__init__(self, **kwargs)
    self.script_observer = ScriptObserver()
    self.base_url = kwargs['base_url']
    self.link_text = kwargs['link_text']
    self.addLogObserver('stdio', self.script_observer)

  def createSummary(self, log):
    url = ('%s/%s' % (self.base_url, self.script_observer.last_change))
    self.addURL(self.link_text, url)
