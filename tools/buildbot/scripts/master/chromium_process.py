#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Source file for buildbot.process module modifications. """

import buildbot.process.base as base
import buildbot.status.progress as progress
import buildbot.status.builder as builder
from twisted.python import log
from log_parser import cl_command

class BuildRequest(base.BuildRequest):
  """ Subclass of the builder.progress.base.BuildRequest that provides
  a way to set additional attributes on the request. We do that to pass custom
  attributes from WebStatus resources to build instance.
  """
  # Additional attributes that we would like to provide.
  params = ['clobber']

  def __init__(self, *args, **kwargs):
    for param in self.params:
      if kwargs.has_key(param):
        setattr(self, param, kwargs.pop(param))

    base.BuildRequest.__init__(self, *args, **kwargs)

class Build(base.Build):

  def _makeCompileClobber(self, step):
    """ Finds compilation step and sets --clobber argument if necessary. """
    clobber = len([request for request in self.requests
                  if hasattr(request, 'clobber') and request.clobber])
    if clobber and isinstance(step, cl_command.CLCommand):
      if not '--clobber' in step.command:
        # Step instances' 'command' object refer to the step factory
        # 'command' object. Modifying step.command array will modify
        # the factory too. Create a deep copy of shell command definition
        # array first and only then work on it.
        command_deep_copy = step.command[:]
        if '--' in command_deep_copy:
          # --clobber needs to come before the --, which signifies the end of
          # options for compile.py and the beginning of options to be passed
          # straight through to the build tool.
          command_deep_copy.insert(command_deep_copy.index('--'), '--clobber')
        else:
          command_deep_copy.append('--clobber')
        step.command = command_deep_copy

  def _handleCustomBuildRequestOptions(self, step):
    """Hook method to modify steps"""
    self._makeCompileClobber(step)

  def setupBuild(self, expectations):
    """Overriden method from buildbot.process.base.Build.

    It does exactly the same thing as an original one except
    that it provides a hook to modify steps.
    """
    # create the actual BuildSteps. If there are any name collisions, we
    # add a count to the loser until it is unique.
    self.steps = []
    self.stepStatuses = {}
    stepnames = []
    sps = []

    for factory, args in self.stepFactories:
      args = args.copy()
      try:
        step = factory(**args)
        # next line is the only change made to the overridden implementation.
        self._handleCustomBuildRequestOptions(step)
      except:
        log.msg("error while creating step, factory=%s, args=%s"
                % (factory, args))
        raise
      step.setBuild(self)
      step.setBuildSlave(self.slavebuilder.slave)
      step.setDefaultWorkdir(self.workdir)
      name = step.name
      count = 1
      while name in stepnames and count < 100:
        count += 1
        name = step.name + "_%d" % count
      if name in stepnames:
        raise RuntimeError("duplicate step '%s'" % step.name)
      step.name = name
      stepnames.append(name)
      self.steps.append(step)

      # tell the BuildStatus about the step. This will create a
      # BuildStepStatus and bind it to the Step.
      step_status = self.build_status.addStepWithName(name)
      step.setStepStatus(step_status)

      sp = None
      if self.useProgress:
        # XXX: maybe bail if step.progressMetrics is empty? or skip
        # progress for that one step (i.e. "it is fast"), or have a
        # separate "variable" flag that makes us bail on progress
        # tracking
        sp = step.setupProgress()
      if sp:
        sps.append(sp)

    # Create a buildbot.status.progress.BuildProgress object. This is
    # called once at startup to figure out how to build the long-term
    # Expectations object, and again at the start of each build to get a
    # fresh BuildProgress object to track progress for that individual
    # build. TODO: revisit at-startup call

    if self.useProgress:
      self.progress = progress.BuildProgress(sps)
      if self.progress and expectations:
        self.progress.setExpectationsFrom(expectations)

    # we are now ready to set up our BuildStatus.
    self.build_status.setSourceStamp(self.source)
    self.build_status.setReason(self.reason)
    print "Got reason %s" % self.reason
    if self.reason.endswith(".diff' try job"):
      # TODO(maruel): This is to skip the first '. This code is a little
      # hackish and will need to be updated one day to use a better way to
      # pass the user to blame for the patch.
      reason = self.reason[1:]
      # It's a trybot patch, alter the blamelist. The reason format is:
      # "user-change.diff" (deprecated) or "user.change.bots.diff".
      i = reason.find('-')
      j = reason.find('.')
      # j can't return -1.
      if j > i and i != -1:
        user = reason[:i]
      else:
        user = reason[:j]
      if user:
        self.build_status.setBlamelist([user])
      else:
        self.build_status.setBlamelist(self.blamelist())
    else:
      self.build_status.setBlamelist(self.blamelist())
    print "Got blame list " + str(self.build_status.blamelist)
    self.build_status.setProgress(self.progress)

    self.results = [] # list of FAILURE, SUCCESS, WARNINGS, SKIPPED
    # overall result, may downgrade after each step
    self.result = builder.SUCCESS
    self.text = [] # list of text string lists (text2)
