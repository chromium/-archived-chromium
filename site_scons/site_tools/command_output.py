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

"""Command output builder for SCons."""


import os
import SCons.Script
import subprocess


def RunCommand(cmdargs, cwdir=None, env=None, echo_output=True):
  """Runs an external command.

  Args:
    cmdargs: A command string, or a tuple containing the command and its
        arguments.
    cwdir: Working directory for the command, if not None.
    env: Environment variables dict, if not None.
    echo_output: If True, output will be echoed to stdout.

  Returns:
    The integer errorlevel from the command.
    The combined stdout and stderr as a string.
  """

  # Force unicode string in the environment to strings.
  if env:
    env = dict([(k, str(v)) for k, v in env.items()])
  child = subprocess.Popen(cmdargs, cwd=cwdir, env=env, shell=True,
                           stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  child_out = []
  child_retcode = None

  # Need to poll the child process, since the stdout pipe can fill.
  while child_retcode is None:
    child_retcode = child.poll()
    new_out = child.stdout.read()
    if echo_output:
      print new_out,
    child_out.append(new_out)

  if echo_output:
    print   # end last line of output
  return child_retcode, ''.join(child_out)


def CommandOutputBuilder(target, source, env):
  """Command output builder.

  Args:
    self: Environment in which to build
    target: List of target nodes
    source: List of source nodes

  Returns:
    None or 0 if successful; nonzero to indicate failure.

  Runs the command specified in the COMMAND_OUTPUT_CMDLINE environment variable
  and stores its output in the first target file.  Additional target files
  should be specified if the command creates additional output files.

  Runs the command in the COMMAND_OUTPUT_RUN_DIR subdirectory.
  """
  env = env.Clone()

  cmdline = env.subst('$COMMAND_OUTPUT_CMDLINE', target=target, source=source)
  cwdir = env.subst('$COMMAND_OUTPUT_RUN_DIR', target=target, source=source)
  if cwdir:
    cwdir = os.path.normpath(cwdir)
    env.AppendENVPath('PATH', cwdir)
    env.AppendENVPath('LD_LIBRARY_PATH', cwdir)
  else:
    cwdir = None

  retcode, output = RunCommand(cmdline, cwdir=cwdir, env=env['ENV'])

  # Save command line output
  output_file = open(str(target[0]), 'w')
  output_file.write(output)
  output_file.close()

  return retcode


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Add the builder and tell it which build environment variables we use.
  action = SCons.Script.Action(CommandOutputBuilder, varlist=[
      'COMMAND_OUTPUT_CMDLINE',
      'COMMAND_OUTPUT_RUN_DIR',
  ])
  builder = SCons.Script.Builder(action = action)
  env.Append(BUILDERS={'CommandOutput': builder})

  # Default command line is to run the first input
  env['COMMAND_OUTPUT_CMDLINE'] = '$SOURCE'

  # TODO(rspangler): add a pseudo-builder which takes an additional command
  # line as an argument.
