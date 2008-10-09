#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
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

"""Defer tool for SCons."""


import os
import sys
import types


__defer_groups = {}


def _InitializeDefer(self):
  """Re-initializes deferred function handling.

  Args:
    self: Parent environment
  """
  # Clear the list of deferred groups
  __defer_groups.clear()


def _ExecuteDefer(self):
  """Executes deferred functions.

  Args:
    self: Parent environment
  """
  # Save directory, so SConscript functions can occur in the right subdirs
  oldcwd = os.getcwd()

  # Loop through deferred functions
  while __defer_groups:
    did_work = False
    for name, group in __defer_groups.items():
      if group.after.intersection(__defer_groups.keys()):
        continue        # Still have dependencies
      if group.func_env_cwd:
        # Run all the functions in our named group
        for func, env, cwd in group.func_env_cwd:
          os.chdir(cwd)
          func(env)
      did_work = True
      del __defer_groups[name]
      break
    if not did_work:
      print 'Error in _ExecuteDefer: dependency cycle detected.'
      for name, group in __defer_groups.items():
        print '   %s after: %s' % (name, group.after)
      # TODO(rspangler): should throw exception?
      sys.exit(1)

  # Restore directory
  os.chdir(oldcwd)


class DeferFunc(object):
  """Named list of functions to be deferred."""

  def __init__(self):
    """Initialize deferred function object."""
    object.__init__(self)
    self.func_env_cwd = []
    self.after = set()


def Defer(self, *args, **kwargs):
  """Adds a deferred function or modifies defer dependencies.

  Args:
    self: Environment in which Defer() was called
    args: Positional arguments
    kwargs: Named arguments

  The deferred function will be passed the environment used to call Defer(),
  and will be executed in the same working directory as the calling SConscript.

  All deferred functions run after all SConscripts.  Additional dependencies
  may be specified with the after= keyword.

  Usage:

    env.Defer(func)
      # Defer func() until after all SConscripts

    env.Defer(func, after=otherfunc)
      # Defer func() until otherfunc() runs

    env.Defer(func, 'bob')
      # Defer func() until after SConscripts, put in group 'bob'

    env.Defer(func2, after='bob')
      # Defer func2() until after all funcs in 'bob' group have run

    env.Defer(func3, 'sam')
      # Defer func3() until after SConscripts, put in group 'sam'

    env.Defer('bob', after='sam')
      # Defer all functions in group 'bob' until after all functions in group
      # 'sam' have run.

    env.Defer(func4, after=['bob', 'sam'])
      # Defer func4() until after all functions in groups 'bob' and 'sam' have
      # run.
  """
  # Get name of group to defer and/or the a function
  name = None
  func = None
  for a in args:
    if isinstance(a, str):
      name = a
    elif isinstance(a, types.FunctionType):
      func = a
  if func and not name:
    name = func.__name__

  # Get list of names and/or functions this function should defer until after
  after = []
  for a in self.Flatten(kwargs.get('after')):
    if isinstance(a, str):
      after.append(a)
    elif isinstance(a, types.FunctionType):
      after.append(a.__name__)
    elif a is not None:
      # Deferring
      # TODO(rspangler): should throw an exception
      print ('Warning: Defer can only wait for functions or names; ignoring'
             'after = ', a)

  # Find the deferred function
  if name not in __defer_groups:
    __defer_groups[name] = DeferFunc()
  group = __defer_groups[name]

  # If we were given a function, also save environment and current directory
  if func:
    group.func_env_cwd.append((func, self, os.getcwd()))

  # Add dependencies for the function
  group.after.update(after)


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  env.AddMethod(_InitializeDefer)
  env.AddMethod(_ExecuteDefer)
  env.AddMethod(Defer)
