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

"""Build tool setup for Windows.

This module is a SCons tool which should be include in the topmost windows
environment.
It is used as follows:
  env = base_env.Clone(tools = ['component_setup'])
  win_env = base_env.Clone(tools = ['target_platform_windows'])
"""


import os
import time
import command_output
import SCons.Script


def WaitForWritable(target, source, env):
  """Waits for the target to become writable.

  Args:
    target: List of target nodes.
    source: List of source nodes.
    env: Environment context.

  Returns:
    Zero if success, nonzero if error.

  This is a necessary hack on Windows, where antivirus software can lock exe
  files briefly after they're written.  This can cause subsequent reads of the
  file by env.Install() to fail.  To prevent these failures, wait for the file
  to be writable.
  """
  target_path = target[0].abspath
  if not os.path.exists(target_path):
    return 0      # Nothing to wait for

  for retries in range(10):
    try:
      f = open(target_path, 'a+b')
      f.close()
      return 0    # Successfully opened file for write, so we're done
    except (IOError, OSError):
      print 'Waiting for access to %s...' % target_path
      time.sleep(1)

  # If we're still here, fail
  print 'Timeout waiting for access to %s.' % target_path
  return 1


def RunManifest(target, source, env, resource_num):
  """Run the Microsoft Visual Studio manifest tool (mt.exe).

  Args:
    target: List of target nodes.
    source: List of source nodes.
    env: Environment context.
    resource_num: Resource number to modify in target (1=exe, 2=dll).

  Returns:
    Zero if success, nonzero if error.

  The mt.exe tool seems to experience intermittent failures trying to write to
  .exe or .dll files.  Antivirus software makes this worse, but the problem
  can still occur even if antivirus software is disabled.  The failures look
  like:

      mt.exe : general error c101008d: Failed to write the updated manifest to
      the resource of file "(name of exe)". Access is denied.

  with mt.exe returning an errorlevel (return code) of 31.  The workaround is
  to retry running mt.exe after a short delay.
  """

  cmdline = env.subst(
      'mt.exe -nologo -manifest ${TARGET}.manifest -outputresource:$TARGET;%d'
      % resource_num,
      target=target, source=source)
  print cmdline

  for retry in range(5):
    # If this is a retry, print a message and delay first
    if retry:
      # mt.exe failed to write to the target file.  Print a warning message,
      # delay 3 seconds, and retry.
      print 'Warning: mt.exe failed to write to %s; retrying.' % target[0]
      time.sleep(3)

    return_code, output = command_output.RunCommand(
        cmdline, env=env['ENV'], echo_output=False)
    if return_code != 31:    # Something other than the intermittent error
      break

  # Pass through output (if any) and return code from manifest
  if output:
    print output
  return return_code


def RunManifestExe(target, source, env):
  """Calls RunManifest for updating an executable (resource_num=1)."""
  return RunManifest(target, source, env, resource_num=1)


def RunManifestDll(target, source, env):
  """Calls RunManifest for updating a dll (resource_num=2)."""
  return RunManifest(target, source, env, resource_num=2)


def ComponentPlatformSetup(env, builder_name):
  """Hook to allow platform to modify environment inside a component builder.

  Args:
    env: Environment to modify
    builder_name: Name of the builder
  """
  if env.get('ENABLE_EXCEPTIONS'):
    env.FilterOut(
        CPPDEFINES=['_HAS_EXCEPTIONS=0'],
        # There are problems with LTCG when some files are compiled with
        # exceptions and some aren't (the v-tables for STL and BOOST classes
        # don't match).  Therefore, turn off LTCG when exceptions are enabled.
        CCFLAGS=['/GL'],
        LINKFLAGS=['/LTCG'],
        ARFLAGS=['/LTCG'],
    )
    env.Append(CCFLAGS=['/EHsc'])

  if builder_name in ('ComponentObject', 'ComponentLibrary'):
    if env.get('COMPONENT_STATIC'):
      env.Append(CPPDEFINES=['_LIB'])
    else:
      env.Append(CPPDEFINES=['_USRDLL', '_WINDLL'])

  if builder_name == 'ComponentTestProgram':
    env.FilterOut(
        CPPDEFINES=['_WINDOWS'],
        LINKFLAGS=['/SUBSYSTEM:WINDOWS'],
    )
    env.Append(
        CPPDEFINES=['_CONSOLE'],
        LINKFLAGS=['/SUBSYSTEM:CONSOLE'],
    )

#------------------------------------------------------------------------------


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Set up environment paths first

  # Load various Visual Studio related tools.
  env.Tool('as')
  env.Tool('msvs')
  env.Tool('windows_hard_link')

  pre_msvc_env = env['ENV'].copy()

  env.Tool('msvc')
  env.Tool('mslib')
  env.Tool('mslink')

  # The msvc, mslink, and mslib tools search the registry for installed copies
  # of Visual Studio and prepends them to the PATH, INCLUDE, and LIB
  # environment variables.  Block these changes if necessary.
  if env.get('MSVC_BLOCK_ENVIRONMENT_CHANGES'):
    env['ENV'] = pre_msvc_env

  # Declare bits
  DeclareBit('windows', 'Target platform is windows.',
             exclusive_groups=('target_platform'))
  env.SetBits('windows')

  env.Replace(
      TARGET_PLATFORM='WINDOWS',
      COMPONENT_PLATFORM_SETUP=ComponentPlatformSetup,

      # A better rebuild command (actually cleans, then rebuild)
      MSVSREBUILDCOM=''.join(['$MSVSSCONSCOM -c "$MSVSBUILDTARGET" && ',
                              '$MSVSSCONSCOM "$MSVSBUILDTARGET"']),
  )

  env.Append(
      HOST_PLATFORMS=['WINDOWS'],
      CPPDEFINES=['OS_WINDOWS=OS_WINDOWS'],

      # Turn up the warning level
      CCFLAGS=['/W3'],

      # Force x86 platform for now
      LINKFLAGS=['/MACHINE:X86'],
      ARFLAGS=['/MACHINE:X86'],

      # Settings for debug
      CCFLAGS_DEBUG=[
          '/Od',     # disable optimizations
          '/RTC1',   # enable fast checks
          '/MTd',    # link with LIBCMTD.LIB debug lib
      ],
      LINKFLAGS_DEBUG=['/DEBUG'],

      # Settings for optimized
      CCFLAGS_OPTIMIZED=[
          '/O1',     # optimize for size
          '/MT',     # link with LIBCMT.LIB (multi-threaded, static linked crt)
          '/GS',     # enable security checks
      ],

      # Settings for component_builders
      COMPONENT_LIBRARY_LINK_SUFFIXES=['.lib'],
      COMPONENT_LIBRARY_DEBUG_SUFFIXES=['.pdb'],
  )

  # Add manifests to EXEs and DLLs
  wait_action = SCons.Script.Action(WaitForWritable,
                                    lambda target, source, env: ''),
  env['LINKCOM'] = [
      env['LINKCOM'],
      SCons.Script.Action(RunManifestExe, lambda target, source, env: ''),
      SCons.Script.Delete('${TARGET}.manifest'),
      wait_action,
  ]
  env['SHLINKCOM'] = [
      env['SHLINKCOM'],
      SCons.Script.Action(RunManifestDll, lambda target, source, env: ''),
      SCons.Script.Delete('${TARGET}.manifest'),
      wait_action,
  ]
  env['WINDOWS_INSERT_MANIFESTS'] = True
  env.Append(LINKFLAGS=['-manifest'])
