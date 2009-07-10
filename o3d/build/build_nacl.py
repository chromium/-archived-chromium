#!/usr/bin/python2.4
# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This file is just here so that we can modify the python path before
# invoking nixysa.

import os
import os.path
import shutil
import subprocess
import sys

third_party = os.path.join('..', '..', 'third_party')
gflags_dir = os.path.join(third_party, 'gflags', 'python')
sys.path.append(gflags_dir)

import gflags

FLAGS = gflags.FLAGS
gflags.DEFINE_string('configuration', 'Debug',
                     'Specify which configuration to build.')

gflags.DEFINE_string('platform', 'win',
                      'Specify which platform to build.')

gflags.DEFINE_boolean('debug', False,
                      'Whether or not to turn on debug output '
                      'when running scons.')

gflags.DEFINE_string('output', '.', 'Sets the output directory.')


def CopyIfNewer(inputs, output_dir):
  '''Copy the inputs to the output directory, but only if the files are
  newer than the destination files.'''
  for input in inputs:
    output = os.path.join(output_dir, os.path.basename(input))
    if os.path.exists(input):
      if (not os.path.exists(output) or
          os.path.getmtime(input) > os.path.getmtime(output)):
        try:
          print 'Copying from %s to %s' % (input, output)
          shutil.copy2(input, output)
        except:
          return False
      else:
        print '%s is up to date.' % output
  return True

def PrependToPath(path, item):
  '''Prepend an item to an environment variable path.'''
  orig_path = os.environ.get(path)
  if orig_path:
    new_path = os.pathsep.join([item, orig_path])
  else:
    new_path = item

  os.environ[path] = new_path

def AppendToPath(path, item):
  '''Append an item to an environment variable path.'''
  orig_path = os.environ.get(path)
  if orig_path:
    new_path = os.pathsep.join([orig_path, item])
  else:
    new_path = item

  os.environ[path] = new_path

def FindPlatformSDK():
  '''On Windows, find the installed location of the Platform SDK.'''
  import _winreg
  try:
    winsdk_key = _winreg.OpenKey(
        _winreg.HKEY_LOCAL_MACHINE,
        'SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\v6.1\\WinSDKBuild')
  except WindowsError:
    try:
      winsdk_key = _winreg.OpenKey(
          _winreg.HKEY_LOCAL_MACHINE,
          'SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\v6.0A\\WinSDKBuild')
    except WindowsError:
      try:
        winsdk_key = _winreg.OpenKey(
            _winreg.HKEY_LOCAL_MACHINE,
            'SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\v6.0A\\VistaClientHeadersLibs')
      except WindowsError:
        print 'The Windows SDK version 6.0 or later needs to be installed'
        sys.exit(1)
  try:
    winsdk_dir, value_type = _winreg.QueryValueEx(winsdk_key,
                                                  'InstallationFolder')
  except WindowsError:
    print 'The Windows SDK version 6.0 or later needs to be installed'
    sys.exit(1)
  _winreg.CloseKey(winsdk_key)

  # Strip off trailing slashes
  winsdk_dir = winsdk_dir.rstrip('\\/')
  AppendToPath('PATH', os.path.join(winsdk_dir, 'Bin'))
  AppendToPath('INCLUDE', os.path.join(winsdk_dir, 'Include'))
  AppendToPath('LIB', os.path.join(winsdk_dir, 'Lib'))

def main(args):
  extra_args = FLAGS(args)
  # strip off argv[0]
  extra_args = extra_args[1:]

  pythonpath = os.path.join(third_party, 'scons', 'scons-local')
  PrependToPath('PYTHONPATH', pythonpath)

  binutils_path = os.path.join(third_party, 'gnu_binutils', 'files')
  AppendToPath('PATH', binutils_path)

  config = 'opt'
  if FLAGS.configuration == 'Debug':
    config = 'dbg'
  elif FLAGS.configuration == 'Release':
    config = 'opt'

  mode = '%s-%s' % (config, FLAGS.platform)

  native_client_dir = os.path.join(third_party, 'native_client',
                                   'googleclient', 'native_client')

  args = [
    'MODE=%s' % mode,
    'naclsdk_validate=0',
    'sdl=none',
    '--verbose',
    '--file=SConstruct',
    '-C',
    native_client_dir,
  ]
  scons = os.path.join(third_party, 'scons', 'scons.py')
  args = [sys.executable, scons] + args + extra_args

  # Add the platform SDK to INCLUDE and LIB
  if FLAGS.platform == 'win':
    FindPlatformSDK()

  print 'Executing %s' % ' '.join(args)
  status = subprocess.call(args)

  # Calculate what the output files should be.
  outputs = []
  for arg in extra_args:
    file_path = os.path.join(native_client_dir,
                             'scons-out', mode, 'lib', arg)
    if FLAGS.platform == 'win':
      file_path = file_path + '.lib'
    else:
      file_path = file_path + '.a'
    outputs.append(file_path)

  if status == 0 and not CopyIfNewer(outputs, FLAGS.output):
    sys.exit(-1)
  else:
    sys.exit(status)

if __name__ == '__main__':
  main(sys.argv)
