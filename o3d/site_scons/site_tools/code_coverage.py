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

"""SCons tool for generating code coverage.

This module enhances a debug environment to add code coverage.
It is used as follows:
  coverage_env = dbg_env.Clone(tools = ['code_coverage'])
"""


def AddCoverageSetup(env):
  """Add coverage related build steps and dependency links.

  Args:
    env: a leaf environment ready to have coverage steps added.
  """
  # Add a step to start coverage (for instance windows needs this).
  # This step should get run before and tests are run.
  if env.get('COVERAGE_START_CMD', None):
    start = env.Command('$COVERAGE_START_FILE', [], '$COVERAGE_START_CMD')
    env.AlwaysBuild(start)
  else:
    start = []

  # Add a step to end coverage (used on basically all platforms).
  # This step should get after all the tests have run.
  if env.get('COVERAGE_STOP_CMD', None):
    stop = env.Command('$COVERAGE_OUTPUT_FILE', [], '$COVERAGE_STOP_CMD')
    env.AlwaysBuild(stop)
  else:
    stop = []

  # start must happen before tests run, stop must happen after.
  for group in env.SubstList2('$COVERAGE_TARGETS'):
    group_alias = env.Alias(group)
    # Force each alias to happen after start but before stop.
    env.Requires(group_alias, start)
    env.Requires(stop, group_alias)
    # Force each source of the aliases to happen after start but before stop.
    # This is needed to work around non-standard aliases in some projects.
    for test in group_alias:
      for s in test.sources:
        env.Requires(s, start)
        env.Requires(stop, s)

  # Add an alias for coverage.
  env.Alias('coverage', [start, stop])


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  env['COVERAGE_ENABLED'] = True

  env.SetDefault(
      # Setup up coverage related tool paths.
      # These can be overridden elsewhere, if needed, to relocate the tools.
      COVERAGE_MCOV='mcov',
      COVERAGE_GENHTML='genhtml',
      COVERAGE_ANALYZER='coverage_analyzer.exe',
      COVERAGE_VSPERFCMD='VSPerfCmd.exe',
      COVERAGE_VSINSTR='vsinstr.exe',

      # Setup coverage related locations.
      COVERAGE_DIR='$TARGET_ROOT/coverage',
      COVERAGE_HTML_DIR='$COVERAGE_DIR/html',
      COVERAGE_START_FILE='$COVERAGE_DIR/start.junk',
      COVERAGE_OUTPUT_FILE='$COVERAGE_DIR/coverage.lcov',

      # The list of aliases containing test execution targets.
      COVERAGE_TARGETS=['run_all_tests'],
  )

  # Add in coverage flags. These come from target_platform_xxx.
  env.Append(
      CCFLAGS='$COVERAGE_CCFLAGS',
      LIBS='$COVERAGE_LIBS',
      LINKFLAGS='$COVERAGE_LINKFLAGS',
      SHLINKFLAGS='$COVERAGE_SHLINKFLAGS',
  )

  # Change the definition of Install if required by the platform.
  if env.get('COVERAGE_INSTALL'):
    env['PRECOVERAGE_INSTALL'] = env['INSTALL']
    env['INSTALL'] = env['COVERAGE_INSTALL']

  # Add any extra paths.
  env.AppendENVPath('PATH', env.SubstList2('$COVERAGE_EXTRA_PATHS'))

  # Add coverage start/stop and processing in deferred steps.
  env.Defer(AddCoverageSetup)
