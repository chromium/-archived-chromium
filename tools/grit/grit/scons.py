#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''SCons integration for GRIT.
'''

# NOTE:  DO NOT IMPORT ANY GRIT STUFF HERE - we import lazily so that grit and
# its dependencies aren't imported until actually needed.

import os
import types

def _IsDebugEnabled():
  return 'GRIT_DEBUG' in os.environ and os.environ['GRIT_DEBUG'] == '1'

def _SourceToFile(source):
  '''Return the path to the source file, given the 'source' argument as provided
  by SCons to the _Builder or _Emitter functions.
  '''
  # Get the filename of the source.  The 'source' parameter can be a string,
  # a "node", or a list of strings or nodes.
  if isinstance(source, types.ListType):
    # TODO(gspencer):  Had to add the .rfile() method to the following
    # line to get this to work with Repository() directories.
    # Get this functionality folded back into the upstream grit tool.
    #source = str(source[0])
    for s in source:
      if str(s.rfile()).endswith('.grd'):
        return str(s.rfile())
  else:
    # TODO(gspencer):  Had to add the .rfile() method to the following
    # line to get this to work with Repository() directories.
    # Get this functionality folded back into the upstream grit tool.
    #source = str(source))
    source = str(source.rfile())
  return source


def _Builder(target, source, env):
  # We fork GRIT into a separate process so we can use more processes between
  # scons and GRIT. This already runs as separate threads, but because of the
  # python GIL, all these threads have to share the same process.  By using
  # fork, we can use multiple processes and processors.
  pid = os.fork()
  if pid != 0:
    os.waitpid(pid, 0)
    return
  from grit import grit_runner
  from grit.tool import build
  options = grit_runner.Options()
  # This sets options to default values.
  options.ReadOptions(['-v'])
  options.input = _SourceToFile(source)

  # TODO(joi) Check if we can get the 'verbose' option from the environment.

  builder = build.RcBuilder()

  # Get the CPP defines from the environment.
  for flag in env.get('RCFLAGS', []):
    if flag.startswith('/D'):
      flag = flag[2:]
    name, val = build.ParseDefine(flag)
    # Only apply to first instance of a given define
    if name not in builder.defines:
      builder.defines[name] = val

  # To ensure that our output files match what we promised SCons, we
  # use the list of targets provided by SCons and update the file paths in
  # our .grd input file with the targets.
  builder.scons_targets = [str(t) for t in target]
  builder.Run(options, [])

  # Exit the child process.
  os._exit(0)


def _Emitter(target, source, env):
  '''A SCons emitter for .grd files, which modifies the list of targes to
  include all files in the <outputs> section of the .grd file as well as
  any other files output by 'grit build' for the .grd file.
  '''
  from grit import grd_reader
  from grit import util

  # TODO(gspencer):  Had to use .abspath, not str(target[0]), to get
  # this to work with Repository() directories.
  # Get this functionality folded back into the upstream grit tool.
  #base_dir = util.dirname(str(target[0]))
  base_dir = util.dirname(target[0].abspath)

  grd = grd_reader.Parse(_SourceToFile(source), debug=_IsDebugEnabled())

  target = []
  lang_folders = {}
  # Add all explicitly-specified output files
  for output in grd.GetOutputFiles():
    path = os.path.join(base_dir, output.GetFilename())
    target.append(path)

    if path.endswith('.h'):
      path, filename = os.path.split(path)
    if _IsDebugEnabled():
      print "GRIT: Added target %s" % path
    if output.attrs['lang'] != '':
      lang_folders[output.attrs['lang']] = os.path.dirname(path)

  # Add all generated files, once for each output language.
  for node in grd:
    if node.name == 'structure':
      # TODO(joi) Should remove the "if sconsdep is true" thing as it is a
      # hack - see grit/node/structure.py
      if node.HasFileForLanguage() and node.attrs['sconsdep'] == 'true':
        for lang in lang_folders:
          path = node.FileForLanguage(lang, lang_folders[lang],
                                      create_file=False,
                                      return_if_not_generated=False)
          if path:
            target.append(path)
            if _IsDebugEnabled():
              print "GRIT: Added target %s" % path

  # return target and source lists
  return (target, source)


def _Scanner(file_node, env, path):
  '''A SCons scanner function for .grd files, which outputs the list of files
  that changes in could change the output of building the .grd file.
  '''
  if not str(file_node.rfile()).endswith('.grd'):
    return []

  from grit import grd_reader
  # TODO(gspencer):  Had to add the .rfile() method to the following
  # line to get this to work with Repository() directories.
  # Get this functionality folded back into the upstream grit tool.
  #grd = grd_reader.Parse(str(file_node)), debug=_IsDebugEnabled())
  grd = grd_reader.Parse(os.path.abspath(_SourceToFile(file_node)),
                         debug=_IsDebugEnabled())
  files = []
  for node in grd:
    if (node.name == 'structure' or node.name == 'skeleton' or
        (node.name == 'file' and node.parent and
         node.parent.name == 'translations')):
      files.append(os.path.abspath(node.GetFilePath()))
    elif node.name == 'include':
      files.append(node.FilenameToOpen())

  # Add in the grit source files.  If one of these change, we want to re-run
  # grit.
  grit_root_dir = env.subst('$CHROME_SRC_DIR/tools/grit')
  for root, dirs, filenames in os.walk(grit_root_dir):
    grit_src = [os.path.join(root, f) for f in filenames if f.endswith('.py')]
    files.extend(grit_src)

  return files


def _BuildStr(targets, sources, env):
  '''This message gets printed each time the builder runs.'''
  return "Running GRIT on %s" % str(sources[0].rfile())


# Function name is mandated by newer versions of SCons.
def generate(env):
  # The varlist parameter tells SCons that GRIT needs to be invoked again
  # if RCFLAGS has changed since last compilation.

  # TODO(gspencer):  change to use the public SCons API Action()
  # and Builder(), instead of reaching directly into internal APIs.
  # Get this change folded back into the upstream grit tool.
  action = env.Action(_Builder, _BuildStr, varlist=['RCFLAGS'])

  scanner = env.Scanner(function=_Scanner, name='GRIT scanner', skeys=['.grd'])

  builder = env.Builder(action=action, emitter=_Emitter,
                        source_scanner=scanner,
                        src_suffix='.grd')

  # add our builder and scanner to the environment
  env.Append(BUILDERS = {'GRIT': builder})

# Function name is mandated by newer versions of SCons.
def exists(env):
  return 1
