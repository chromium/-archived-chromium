# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tool module for adding, to a construction environment, Chromium-specific
wrappers around Hammer builders.  This gives us a central place for any
customization we need to make to the different things we build.
"""

import sys

from SCons.Script import *

import SCons.Node
import _Node_MSVS as MSVS

class Null(object):
  def __new__(cls, *args, **kwargs):
    if '_inst' not in vars(cls):
      cls._inst = super(type, cls).__new__(cls, *args, **kwargs)
    return cls._inst
  def __init__(self, *args, **kwargs): pass
  def __call__(self, *args, **kwargs): return self
  def __repr__(self): return "Null()"
  def __nonzero__(self): return False
  def __getattr__(self, name): return self
  def __setattr__(self, name, val): return self
  def __delattr__(self, name): return self
  def __getitem__(self, name): return self

class FileList(object):
  def __init__(self, entries=None):
    if isinstance(entries, FileList):
      entries = entries.entries
    self.entries = entries or []
  def __getitem__(self, i):
    return self.entries[i]
  def __setitem__(self, i, item):
    self.entries[i] = item
  def __delitem__(self, i):
    del self.entries[i]
  def __add__(self, other):
    if isinstance(other, FileList):
      return self.__class__(self.entries + other.entries)
    elif isinstance(other, type(self.entries)):
      return self.__class__(self.entries + other)
    else:
      return self.__class__(self.entries + list(other))
  def __radd__(self, other):
    if isinstance(other, FileList):
      return self.__class__(other.entries + self.entries)
    elif isinstance(other, type(self.entries)):
      return self.__class__(other + self.entries)
    else:
      return self.__class__(list(other) + self.entries)
  def __iadd__(self, other):
    if isinstance(other, FileList):
      self.entries += other.entries
    elif isinstance(other, type(self.entries)):
      self.entries += other
    else:
      self.entries += list(other)
    return self
  def append(self, item):
    return self.entries.append(item)
  def extend(self, item):
    return self.entries.extend(item)
  def index(self, item, *args):
    return self.entries.index(item, *args)
  def remove(self, item):
    return self.entries.remove(item)

def FileListWalk(top, topdown=True, onerror=None):
  """
  """
  try:
    entries = top.entries
  except AttributeError, err:
    if onerror is not None:
      onerror(err)
    return

  dirs, nondirs = [], []
  for entry in entries:
    if hasattr(entry, 'entries'):
      dirs.append(entry)
    else:
      nondirs.append(entry)

  if topdown:
    yield top, dirs, nondirs
  for entry in dirs:
    for x in FileListWalk(entry, topdown, onerror):
      yield x
  if not topdown:
    yield top, dirs, nondirs

class ChromeFileList(FileList):
  def Append(self, *args):
    for element in args:
      self.append(element)
  def Extend(self, *args):
    for element in args:
      self.extend(element)
  def Remove(self, *args):
    for top, lists, nonlists in FileListWalk(self, topdown=False):
      for element in args:
        try:
          top.remove(element)
        except ValueError:
          pass
  def Replace(self, old, new):
    for top, lists, nonlists in FileListWalk(self, topdown=False):
      try:
        i = top.index(old)
      except ValueError:
        pass
      else:
        top[i] = new


def FilterOut(self, **kw):
  """Removes values from existing construction variables in an Environment.

  The values to remove should be a list.  For example:

  self.FilterOut(CPPDEFINES=['REMOVE_ME', 'ME_TOO'])

  Args:
    self: Environment to alter.
    kw: (Any other named arguments are values to remove).
  """

  kw = SCons.Environment.copy_non_reserved_keywords(kw)
  for key, val in kw.items():
    envval = self.get(key, None)
    if envval is None:
      # No existing variable in the environment, so nothing to delete.
      continue

    for vremove in val:
      # Use while not if, so we can handle duplicates.
      while vremove in envval:
        envval.remove(vremove)

    self[key] = envval

    # TODO(sgk): SCons.Environment.Append() has much more logic to deal
    # with various types of values.  We should handle all those cases in here
    # too.  (If variable is a dict, etc.)


import __builtin__
__builtin__.ChromeFileList = ChromeFileList

non_compilable_suffixes = {
    'LINUX' : set([
        '.bdic',
        '.css',
        '.dat',
        '.h',
        '.html',
        '.hxx',
        '.idl',
        '.js',
        '.rc',
    ]),
    'WINDOWS' : set([
        '.h',
        '.dat',
        '.idl',
    ]),
}

def compilable(env, file):
  base, ext = os.path.splitext(str(file))
  if ext in non_compilable_suffixes[env['TARGET_PLATFORM']]:
    return False
  return True

def compilable_files(env, sources):
  if not hasattr(sources, 'entries'):
    return [x for x in sources if compilable(env, x)]
  result = []
  for top, folders, nonfolders in FileListWalk(sources):
    result.extend([x for x in nonfolders if compilable(env, x)])
  return result

def ChromeProgram(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  if env.get('_GYP'):
    prog = env.Program(target, source, *args, **kw)
    result = env.Install('$DESTINATION_ROOT', prog)
  else:
    result = env.ComponentProgram(target, source, *args, **kw)
  if env.get('INCREMENTAL'):
    env.Precious(result)
  return result

def ChromeTestProgram(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  if env.get('_GYP'):
    prog = env.Program(target, source, *args, **kw)
    result = env.Install('$DESTINATION_ROOT', prog)
  else:
    result = env.ComponentTestProgram(target, source, *args, **kw)
  if env.get('INCREMENTAL'):
    env.Precious(*result)
  return result

def ChromeLibrary(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  if env.get('_GYP'):
    lib = env.Library(target, source, *args, **kw)
    result = env.Install('$LIB_DIR', lib)
  else:
    result = env.ComponentLibrary(target, source, *args, **kw)
  return result

def ChromeLoadableModule(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  if env.get('_GYP'):
    result = env.LoadableModule(target, source, *args, **kw)
  else:
    kw['COMPONENT_STATIC'] = True
    result = env.LoadableModule(target, source, *args, **kw)
  return result

def ChromeStaticLibrary(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  if env.get('_GYP'):
    lib = env.StaticLibrary(target, source, *args, **kw)
    result = env.Install('$LIB_DIR', lib)
  else:
    kw['COMPONENT_STATIC'] = True
    result = env.ComponentLibrary(target, source, *args, **kw)
  return result

def ChromeSharedLibrary(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  if env.get('_GYP'):
    lib = env.SharedLibrary(target, source, *args, **kw)
    result = env.Install('$LIB_DIR', lib)
  else:
    kw['COMPONENT_STATIC'] = False
    result = [env.ComponentLibrary(target, source, *args, **kw)[0]]
  if env.get('INCREMENTAL'):
    env.Precious(result)
  return result

def ChromeObject(env, *args, **kw):
  if env.get('_GYP'):
    result = env.Object(target, source, *args, **kw)
  else:
    result = env.ComponentObject(*args, **kw)
  return result

def ChromeMSVSFolder(env, *args, **kw):
  if not env.Bit('msvs'):
    return Null()
  return env.MSVSFolder(*args, **kw)

def ChromeMSVSProject(env, *args, **kw):
  if not env.Bit('msvs'):
    return Null()
  try:
    dest = kw['dest']
  except KeyError:
    dest = None
  else:
    del kw['dest']
  result = env.MSVSProject(*args, **kw)
  env.AlwaysBuild(result)
  if dest:
    i = env.Command(dest, result, Copy('$TARGET', '$SOURCE'))
    Alias('msvs', i)
  return result

def ChromeMSVSSolution(env, *args, **kw):
  if not env.Bit('msvs'):
    return Null()
  try:
    dest = kw['dest']
  except KeyError:
    dest = None
  else:
    del kw['dest']
  result = env.MSVSSolution(*args, **kw)
  env.AlwaysBuild(result)
  if dest:
    i = env.Command(dest, result, Copy('$TARGET', '$SOURCE'))
    Alias('msvs', i)
  return result

def generate(env):
  env.AddMethod(ChromeProgram)
  env.AddMethod(ChromeTestProgram)
  env.AddMethod(ChromeLibrary)
  env.AddMethod(ChromeLoadableModule)
  env.AddMethod(ChromeStaticLibrary)
  env.AddMethod(ChromeSharedLibrary)
  env.AddMethod(ChromeObject)
  env.AddMethod(ChromeMSVSFolder)
  env.AddMethod(ChromeMSVSProject)
  env.AddMethod(ChromeMSVSSolution)

  env.AddMethod(FilterOut)

  # Add the grit tool to the base environment because we use this a lot.
  sys.path.append(env.Dir('$SRC_DIR/tools/grit').abspath)
  env.Tool('scons', toolpath=[env.Dir('$SRC_DIR/tools/grit/grit')])

  # Add the repack python script tool that we use in multiple places.
  sys.path.append(env.Dir('$SRC_DIR/tools/data_pack').abspath)
  env.Tool('scons', toolpath=[env.Dir('$SRC_DIR/tools/data_pack/')])

def exists(env):
  return True
