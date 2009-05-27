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

"""Simple DirectMedia Layer tool for SCons.

This tool sets up an environment to use the SDL library.
"""

import os
import sys
import SCons.Script


def _HermeticSDL(env):
  """Set things up if sdl is hermetically setup somewhere."""

  if sys.platform in ['win32', 'cygwin']:
    env.SetDefault(
        SDL_DIR='$SDL_HERMETIC_WINDOWS_DIR',
        SDL_CPPPATH=['$SDL_DIR/include'],
        SDL_LIBPATH=['$SDL_DIR/lib'],
        SDL_LIBS=['SDL', 'SDLmain'],
        SDL_FRAMEWORKPATH=[],
        SDL_FRAMEWORKS=[],
    )
  elif sys.platform in ['darwin']:
    env.SetDefault(
        SDL_DIR='$SDL_HERMETIC_MAC_DIR',
        SDL_CPPPATH=[
            '$SDL_DIR/SDL.framework/Headers',
        ],
        SDL_LIBPATH=[],
        SDL_LIBS=[],
        SDL_FRAMEWORKPATH=['$SDL_DIR'],
        SDL_FRAMEWORKS=['SDL', 'Cocoa'],
    )
  elif sys.platform in ['linux', 'linux2', 'posix']:
    env.SetDefault(
        SDL_DIR='$SDL_HERMETIC_LINUX_DIR',
        SDL_CPPPATH='$SDL_DIR/include',
        SDL_LIBPATH='$SDL_DIR/lib',
        SDL_LIBS=['SDL', 'SDLmain'],
        SDL_FRAMEWORKPATH=[],
        SDL_FRAMEWORKS=[],
    )
  else:
    env.SetDefault(
        SDL_VALIDATE_PATHS=[
            ('unsupported_platform',
             ('Not supported on this platform.',)),
        ],
        SDL_IS_MISSING=True,
    )

  if not env.get('SDL_IS_MISSING', False):
    env.SetDefault(
        SDL_VALIDATE_PATHS=[
            ('$SDL_DIR',
             ('You are missing a hermetic copy of SDL...',)),
        ],
    )


def _LocalSDL(env):
  """Set things up if sdl is locally installed."""

  if sys.platform in ['win32', 'cygwin']:
    env.SetDefault(
        SDL_DIR='c:/SDL-1.2.13',
        SDL_CPPPATH='$SDL_DIR/include',
        SDL_LIBPATH='$SDL_DIR/lib',
        SDL_LIBS=['SDL', 'SDLmain'],
        SDL_FRAMEWORKPATH=[],
        SDL_FRAMEWORKS=[],
        SDL_VALIDATE_PATHS=[
            ('$SDL_DIR',
             ('You are missing SDL-1.2.13 on your system.',
              'It was supposed to be in: ${SDL_DIR}',
              'You can download it from:',
              '  http://www.libsdl.org/download-1.2.php')),
        ],
    )
  elif sys.platform in ['darwin']:
    env.SetDefault(
        SDL_CPPPATH=['/Library/Frameworks/SDL.framework/Headers'],
        SDL_LIBPATH=[],
        SDL_LIBS=[],
        SDL_FRAMEWORKPATH=[],
        SDL_FRAMEWORKS=['SDL', 'Cocoa'],
        SDL_VALIDATE_PATHS=[
            ('/Library/Frameworks/SDL.framework/SDL',
             ('You are missing the SDL framework on your system.',
              'You can download it from:',
              'http://www.libsdl.org/download-1.2.php')),
        ],
    )
  elif sys.platform in ['linux', 'linux2', 'posix']:
    env.SetDefault(
        SDL_CPPPATH='/usr/include/SDL',
        SDL_LIBPATH='/usr/lib',
        SDL_LIBS=['SDL', 'SDLmain'],
        SDL_FRAMEWORKPATH=[],
        SDL_FRAMEWORKS=[],
        SDL_VALIDATE_PATHS=[
            ('/usr/lib/libSDL.so',
             ('You are missing SDL on your system.',
              'Run sudo apt-get install libsdl1.2-dev.')),
        ],
    )


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Default to hermetic mode
  # TODO: Should default to local for open-source installs
  env.SetDefault(SDL_MODE='hermetic')

  # Allow the hermetic copy to be disabled on the command line.
  sdl_mode = SCons.Script.ARGUMENTS.get('sdl', env.subst('$SDL_MODE'))
  env['SDL_MODE'] = sdl_mode
  if sdl_mode == 'local':
    _LocalSDL(env)
  elif sdl_mode == 'hermetic':
    _HermeticSDL(env)
  elif sdl_mode == 'none':
    return
  else:
    assert False

  validate_paths = env['SDL_VALIDATE_PATHS']

  # TODO: Should just print a warning if SDL isn't installed.  Perhaps check
  # for an env['SDL_EXIT_IF_NOT_FOUND'] variable so that projects can decide
  # whether to fail if SDL is missing.
  if not validate_paths:
    sys.stderr.write('*' * 77 + '\n')
    sys.stderr.write('ERROR - SDL not supported on this platform.\n')
    sys.stderr.write('*' * 77 + '\n')
    sys.exit(-1)

  for i in validate_paths:
    if not os.path.exists(env.subst(i[0])):
      sys.stderr.write('*' * 77 + '\n')
      for j in i[1]:
        sys.stderr.write(env.subst(j) + '\n')
      sys.stderr.write('*' * 77 + '\n')
      sys.exit(-1)

  env.Append(
      CPPPATH=['$SDL_CPPPATH'],
      LIBPATH=['$SDL_LIBPATH'],
      LIBS=['$SDL_LIBS'],
      FRAMEWORKPATH=['$SDL_FRAMEWORKPATH'],
      FRAMEWORKS=['$SDL_FRAMEWORKS'],
  )
