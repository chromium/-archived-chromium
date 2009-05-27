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

"""Build tool setup for Linux.

This module is a SCons tool which should be include in the topmost windows
environment.
It is used as follows:
  env = base_env.Clone(tools = ['component_setup'])
  linux_env = base_env.Clone(tools = ['target_platform_linux'])
"""


def ComponentPlatformSetup(env, builder_name):
  """Hook to allow platform to modify environment inside a component builder.

  Args:
    env: Environment to modify
    builder_name: Name of the builder
  """
  if env.get('ENABLE_EXCEPTIONS'):
    env.FilterOut(CCFLAGS=['-fno-exceptions'])
    env.Append(CCFLAGS=['-fexceptions'])

#------------------------------------------------------------------------------


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Preserve some variables that get blown away by the tools.
  saved = dict()
  for k in ['CFLAGS', 'CCFLAGS', 'CXXFLAGS', 'LINKFLAGS', 'LIBS']:
    saved[k] = env.get(k, [])
    env[k] = []

  # Use g++
  env.Tool('g++')
  env.Tool('gcc')
  env.Tool('gnulink')
  env.Tool('ar')
  env.Tool('as')

  # Set target platform bits
  env.SetBits('linux', 'posix')

  env.Replace(
      TARGET_PLATFORM='LINUX',
      COMPONENT_PLATFORM_SETUP=ComponentPlatformSetup,
      CCFLAG_INCLUDE='-include',     # Command line option to include a header

      # Code coverage related.
      COVERAGE_CCFLAGS=['-ftest-coverage', '-fprofile-arcs'],
      COVERAGE_LIBS='gcov',
      COVERAGE_STOP_CMD=[
          '$COVERAGE_MCOV --directory "$TARGET_ROOT" --output "$TARGET"',
          ('$COVERAGE_GENHTML --output-directory $COVERAGE_HTML_DIR '
           '$COVERAGE_OUTPUT_FILE'),
      ],
  )

  env.Append(
      HOST_PLATFORMS=['LINUX'],
      CPPDEFINES=['OS_LINUX=OS_LINUX'],

      # Settings for debug
      CCFLAGS_DEBUG=[
          '-O0',     # turn off optimizations
          '-g',      # turn on debugging info
      ],

      # Settings for optimized
      CCFLAGS_OPTIMIZED=['-O2'],

      # Settings for component_builders
      COMPONENT_LIBRARY_LINK_SUFFIXES=['.so', '.a'],
      COMPONENT_LIBRARY_DEBUG_SUFFIXES=[],
  )

  # Restore saved flags.
  env.Append(**saved)
