#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Subclasses of various slave command classes."""

import copy
import os
from buildbot.steps import source
from buildbot.process import buildstep
from buildbot import slave
from buildbot.steps import shell
from log_parser import process_log

class GoogleSVN(source.SVN):

  def __init__(self, svnurl=None, baseURL=None, defaultBranch=None,
               directory=None, rm_timeout=None, **kwargs):
    self.rm_timeout = rm_timeout
    slave.SVN.__init__(self, svnurl, baseURL, defaultBranch, directory,
                      **kwargs)

  def startVC(self, branch, revision, patch):
    """Just like the base step.SVN, except for the command call at the end.

    This is functionally identical to the method in the base class, but it
    ignores the portions of the latter that were only needed to handle old
    slave versions, and it calls the chromium_commands.SVN registered command
    at the very end.
    """
    warnings = []

    if self.rm_timeout:
      self.args['rm_timeout'] = self.rm_timeout

    if self.svnurl:
      assert not branch # we need baseURL= to use branches
      self.args['svnurl'] = self.svnurl
    else:
      self.args['svnurl'] = self.baseURL + branch
    self.args['revision'] = revision
    self.args['patch'] = patch

    revstuff = []
    if branch is not None and branch != self.branch:
      revstuff.append('[branch]')
    if revision is not None:
      revstuff.append('r%s' % revision)
    self.description.extend(revstuff)
    self.descriptionDone.extend(revstuff)

    cmd = buildstep.LoggedRemoteCommand('svn_google', self.args)
    self.startCommand(cmd, warnings)


class GClient(source.Source):
  """Check out a source tree using gclient."""

  name = 'gclient'

  def __init__(self, svnurl=None, rm_timeout=None, gclient_spec=None,
               **kwargs):
    source.Source.__init__(self, **kwargs)
    self.args['rm_timeout'] = rm_timeout
    self.args['svnurl'] = svnurl
    # linux doesn't handle spaces in command line args properly so remove them.
    # This doesn't matter for the format of the DEPS file.
    self.args['gclient_spec'] = gclient_spec.replace(' ', '')

  def computeSourceRevision(self, changes):
    """Finds the latest revision number from the changeset that have
    triggered the build.

    This is a hook method provided by the parent source.Source class and
    default implementation in source.Source returns None. Return value of this
    method is be used to set 'revsion' argument value for startVC() method."""
    if not changes:
      return None
    lastChange = max([int(c.revision) for c in changes])
    return lastChange

  def startVC(self, branch, revision, patch):
    """
    """
    warnings = []
    args = copy.copy(self.args)
    args['revision'] = revision
    args['branch'] = branch
    args['patch'] = patch
    cmd = buildstep.LoggedRemoteCommand('gclient', args)
    self.startCommand(cmd, warnings)


class ProcessLogShellStep(shell.ShellCommand):
  """ Step that can process log files.

    Delegates actual processing to log_processor, which is a subclass of
    process_log.PerformanceLogParser.

    Sample usage:
    # construct class that will have no-arg constructor.
    log_processor_class = chromium_utils.PartiallyInitialize(
        process_log.PageCyclerLogProcessor,
        report_link='http://host:8010/report.html,
        output_dir='~/www')
    # We are partially constructing Step because the step final
    # initialization is done by BuildBot.
    step = chromium_utils.PartiallyInitialize(
        chromium_step.ProcessLogShellStep,
        log_processor_class=log_processor_class)

  """
  def  __init__(self, log_processor_class, *args, **kwargs):
    """
    Args:
      log_processor_class: subclass of
        process_log.PerformanceLogProcessor that will be initialized and
        invoked once command was successfully completed.
    """
    self._result_text = []
    self._log_processor = log_processor_class()
    shell.ShellCommand.__init__(self, *args, **kwargs)

  def start(self):
    """Overridden shell.ShellCommand.start method.

    Adds a link for the activity that points to report ULR.
    """
    self._CreateReportLinkIfNeccessary()
    shell.ShellCommand.start(self)

  def _GetRevision(self):
    """Returns the revision number for the build.

    Result is the revision number of the latest change that went in
    while doing gclient sync. If None, will return -1 instead.
    """
    if self.build.getProperty('got_revision'):
      return self.build.getProperty('got_revision')
    return -1

  def commandComplete(self, cmd):
    """Callback implementation that will use log process to parse 'stdio' data.
    """
    self._result_text = self._log_processor.Process(
        self._GetRevision(), self.getLog('stdio').getText())

  def getText(self, cmd, results):
    text_list = self.describe(True)
    if self._result_text:
      self._result_text.insert(0, '<div class="BuildResultInfo">')
      self._result_text.append('</div>')
      text_list = text_list + self._result_text
    return text_list

  def _CreateReportLinkIfNeccessary(self):
    if self._log_processor.ReportLink():
      self.addURL('results', "%s" % self._log_processor.ReportLink())
