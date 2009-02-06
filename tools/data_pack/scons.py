#!/usr/bin/python
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

def _SConsNodeToFile(file_node):
  '''Convert a scons file Node object to a path on disk.'''
  return str(file_node.rfile())


def _Build(target, source, env):
  '''Run the repack script.'''
  data_pack_root_dir = env.subst('$CHROME_SRC_DIR/tools/data_pack')
  sys.path.append(data_pack_root_dir)
  import repack
  sources = [_SConsNodeToFile(s) for s in source]
  repack.RePack(_SConsNodeToFile(target[0]), sources)


def _BuildStr(targets, sources, env):
  '''This message gets printed each time the builder runs.'''
  return "Repacking data files into %s" % str(targets[0].rfile())


def _Scanner(file_node, env, path):
  '''Repack files if repack.py or data_pack.py have changed.'''
  data_pack_root_dir = env.subst('$CHROME_SRC_DIR/tools/data_pack')

  files = []
  for f in ('repack.py', 'data_pack.py'):
    files.append(os.path.join(data_pack_root_dir, f))
  return files


#############################################################################
## SCons Tool api methods below.
def generate(env):
  action = env.Action(_Build, _BuildStr)
  scanner = env.Scanner(function=_Scanner, skeys=['.pak'])

  builder = env.Builder(action=action,
                        source_scanner=scanner,
                        src_suffix='.pak')

  # add our builder and scanner to the environment
  env.Append(BUILDERS = {'Repack': builder})


# Function name is mandated by newer versions of SCons.
def exists(env):
  return 1
