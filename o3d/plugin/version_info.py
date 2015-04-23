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

# This script does substitution on a list of files for
# version-specific information relating to the plugin.

import os.path
import re
import sys

gflags_dir = os.path.join('..', '..', 'third_party', 'gflags', 'python')
sys.path.append(gflags_dir)

import gflags

FLAGS = gflags.FLAGS
gflags.DEFINE_boolean('kill_switch', False,
                      'Generate version numbers for kill switch binary.')

gflags.DEFINE_boolean('name', False,
                      'Print out the plugin name and exit.')

gflags.DEFINE_boolean('description', False,
                      'Print out the plugin description and exit.')

gflags.DEFINE_boolean('mimetype', False,
                      'Print out the plugin mime type and exit.')

gflags.DEFINE_boolean('version', False,
                      'Print out the plugin version and exit.')


def GetDotVersion(version):
  return '%d.%d.%d.%d' % version

def GetCommaVersion(version):
  return '%d,%d,%d,%d' % version

def DoReplace(in_filename, out_filename, replacements):
  '''Replace the version number in the given filename with the replacements.'''
  if not os.path.exists(in_filename):
    raise Exception(r'''Input template file %s doesn't exist.''' % file)
  input_file = open(in_filename, 'r')
  input = input_file.read()
  input_file.close()
  for (source, target) in replacements:
    input = re.sub(source, target, input)

  output_file = open(out_filename, 'wb')
  output_file.write(input)
  output_file.close()

def main(argv):
  try:
    files = FLAGS(argv)  # Parse flags
  except gflags.FlagsError, e:
    print '%s.\nUsage: %s [<options>] [<input_file> <output_file>]\n%s' % \
          (e, sys.argv[0], FLAGS)
    sys.exit(1)

  # Strip off argv[0]
  files = files[1:]

  # Get version string from o3d_version.py
  o3d_version_vars = {}
  if FLAGS.kill_switch:
    execfile('../installer/win/o3d_kill_version.py', o3d_version_vars)
  else:
    execfile('../installer/win/o3d_version.py', o3d_version_vars)

  plugin_version = o3d_version_vars['plugin_version']
  sdk_version = o3d_version_vars['sdk_version']

  # This name is used by Javascript to find the plugin therefore it must
  # not change. If you change this you must change the name in
  # samples/o3djs/util.js but be aware, changing the name
  # will break all apps that use o3d on the web.
  O3D_PLUGIN_NAME = 'O3D Plugin'
  O3D_PLUGIN_VERSION = GetDotVersion(plugin_version)
  O3D_PLUGIN_VERSION_COMMAS = GetCommaVersion(plugin_version)
  O3D_SDK_VERSION = GetDotVersion(sdk_version)
  O3D_SDK_VERSION_COMMAS = GetCommaVersion(sdk_version)
  O3D_PLUGIN_DESCRIPTION = '%s version:%s' % (O3D_PLUGIN_NAME,
                                              O3D_PLUGIN_VERSION)
  O3D_PLUGIN_MIME_TYPE = 'application/vnd.o3d.auto'

  if FLAGS.name:
    print '%s' % O3D_PLUGIN_NAME
    sys.exit(0)

  if FLAGS.description:
    print '%s' % O3D_PLUGIN_DESCRIPTION
    sys.exit(0)

  if FLAGS.mimetype:
    print '%s' % O3D_PLUGIN_MIME_TYPE
    sys.exit(0)

  if FLAGS.version:
    print '%s' % O3D_PLUGIN_VERSION
    sys.exit(0)

  plugin_replace_strings = [
      ('@@@PluginName@@@', O3D_PLUGIN_NAME),
      ('@@@ProductVersionCommas@@@', O3D_PLUGIN_VERSION_COMMAS),
      ('@@@ProductVersion@@@', O3D_PLUGIN_VERSION),
      ('@@@PluginDescription@@@', O3D_PLUGIN_DESCRIPTION),
      ('@@@PluginMimeType@@@', O3D_PLUGIN_MIME_TYPE),
  ]

  if len(files) == 2:
    DoReplace(files[0], files[1], plugin_replace_strings)
  elif len(files) > 0:
    raise Exception(r'You must supply and input and output filename for '
                    r'replacement.')

if __name__ == '__main__':
  main(sys.argv)
