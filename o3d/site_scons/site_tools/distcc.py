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

"""Distcc support for SCons.

Since this modifies the C compiler strings, it must be specified after the
compiler tool in the tool list.

Distcc support can be enabled by specifying --distcc on the SCons command
line.
"""


import optparse
import os
import sys
from SCons.compat._scons_optparse import OptionConflictError
import SCons.Script


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""
  if not env.Detect('distcc'):
    return

  try:
    SCons.Script.AddOption(
        '--distcc',
        dest='distcc',
        action='store_true',
        help='enable distcc support')
    SCons.Script.Help('  --distcc                    Enable distcc suport.\n')

  except (OptionConflictError, optparse.OptionConflictError):
    # The distcc tool can be specified for multiple platforms, but the
    # --distcc option can only be added once.  Ignore the error which
    # results from trying to add it a second time.
    pass

  # If distcc isn't enabled, stop now
  if not env.GetOption('distcc'):
    return

  # Copy DISTCC_HOSTS and HOME environment variables from system environment
  for envvar in ('DISTCC_HOSTS', 'HOME'):
    value = env.get(envvar, os.environ.get(envvar))
    if not value:
      print 'Warning: %s not set in environment; disabling distcc.' % envvar
      return
    env['ENV'][envvar] = value

  # Set name of distcc tool
  env['DISTCC'] = 'distcc'

  # Modify compilers we support
  distcc_compilers = env.get('DISTCC_COMPILERS', ['cc', 'gcc', 'c++', 'g++'])
  for compiler_var in ('CC', 'CXX'):
    compiler = env.get(compiler_var)
    if compiler in distcc_compilers:
      if sys.platform == 'darwin':
        # On Mac, distcc requires the full path to the compiler
        compiler = env.WhereIs(compiler)
      env[compiler_var] = '$DISTCC ' + compiler
