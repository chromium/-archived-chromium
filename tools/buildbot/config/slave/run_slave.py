#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Initialize the environment variables and start the buildbot slave.
"""

import os
import shutil
import sys

def remove_all_vars_except(dict, vars):
  """Remove all variable keys from the specified dict except those in vars"""
  for key in set(dict.keys()) - set(vars):
    dict.pop(key)

if '__main__' == __name__:
  # change the current directory to the directory of the script.
  os.chdir(sys.path[0])

  # Set the python path.
  parent_dir = os.path.abspath(os.path.pardir)

  sys.path.insert(0, os.path.join(parent_dir, 'scripts', 'common'))
  sys.path.insert(0, os.path.join(parent_dir, 'scripts', 'private'))
  sys.path.insert(0, os.path.join(parent_dir, 'scripts', 'slave'))
  sys.path.insert(0, os.path.join(parent_dir, 'pylibs'))
  sys.path.insert(0, os.path.join(parent_dir, 'symsrc'))

  # Copy these paths into the PYTHONPATH env variable. If you add
  # more path to sys.path, you need to change this line to reflect the
  # change.
  os.environ['PYTHONPATH'] = os.pathsep.join(sys.path[:4])
  os.environ['CHROME_HEADLESS'] = '1'

  # Platform-specific initialization.

  if sys.platform == 'win32':
    # list of all variables that we want to keep
    env_var = [
        'APPDATA',
        'CHROME_HEADLESS',
        'CHROMIUM_BUILD',
        'COMSPEC',
        'OS',
        'PATH',
        'PATHEXT',
        'PROGRAMFILES',
        'PYTHONPATH',
        'SYSTEMDRIVE',
        'SYSTEMROOT',
        'TEMP',
        'TMP',
        'USERNAME',
        'USERPROFILE',
        'WINDIR',
    ]

    remove_all_vars_except(os.environ, env_var)

    # extend the env variables with the chrome-specific settings.
    depot_tools = os.path.join(parent_dir, 'depot_tools')
    python_24 = os.path.join(depot_tools, 'release', 'python_24')
    system32 = os.path.join(os.environ['SYSTEMROOT'], 'system32')
    wbem = os.path.join(system32, 'WBEM')

    slave_path = [depot_tools, python_24, system32, wbem]
    os.environ['PATH'] = os.pathsep.join(slave_path)
    os.environ['LOGNAME'] = os.environ['USERNAME']

  elif sys.platform in ('darwin', 'posix', 'linux2'):
    # list of all variables that we want to keep
    env_var = [
        'CHROME_HEADLESS',
        'DISPLAY',
        'HOME',
        'HOSTNAME',
        'LOGNAME',
        'PATH',
        'PWD',
        'PYTHONPATH',
        'SHELL',
        'USER',
        'USERNAME'
    ]

    remove_all_vars_except(os.environ, env_var)

    depot_tools = os.path.join(parent_dir, 'depot_tools')

    slave_path = [depot_tools, '/usr/bin', '/bin',
                  '/usr/sbin', '/sbin', '/usr/local/bin']
    os.environ['PATH'] = os.pathsep.join(slave_path)

  # Run the slave.
  import twisted.scripts.twistd as twistd
  twistd.run()
