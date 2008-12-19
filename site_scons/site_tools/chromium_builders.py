# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tool module for adding, to a construction environment, Chromium-specific
wrappers around Hammer builders.  This gives us a central place for any
customization we need to make to the different things we build.
"""

def generate(env):
  def ChromeProgram(env, *args, **kw):
    result = env.ComponentProgram(*args, **kw)
    if env.get('INCREMENTAL'):
      env.Precious(result)
    return result
  env.AddMethod(ChromeProgram)

  def ChromeTestProgram(env, *args, **kw):
    result = env.ComponentTestProgram(*args, **kw)
    if env.get('INCREMENTAL'):
      env.Precious(*result)
    return result
  env.AddMethod(ChromeTestProgram)

  def ChromeStaticLibrary(env, *args, **kw):
    kw['COMPONENT_STATIC'] = True
    return env.ComponentLibrary(*args, **kw)
  env.AddMethod(ChromeStaticLibrary)

  def ChromeSharedLibrary(env, *args, **kw):
    kw['COMPONENT_STATIC'] = False
    result = [env.ComponentLibrary(*args, **kw)[0]]
    if env.get('INCREMENTAL'):
      env.Precious(result)
    return result
  env.AddMethod(ChromeSharedLibrary)

  def ChromeObject(env, *args, **kw):
    return env.ComponentObject(*args, **kw)
  env.AddMethod(ChromeObject)

  def ChromeMSVSFolder(env, *args, **kw):
    if env.Bit('msvs'):
      return env.MSVSFolder(*args, **kw)
    return []
  env.AddMethod(ChromeMSVSFolder)

  def ChromeMSVSProject(env, *args, **kw):
    if env.Bit('msvs'):
      return env.MSVSProject(*args, **kw)
    return []
  env.AddMethod(ChromeMSVSProject)

  def ChromeMSVSSolution(env, *args, **kw):
    if env.Bit('msvs'):
      return env.MSVSSolution(*args, **kw)
    return []
  env.AddMethod(ChromeMSVSSolution)

def exists(env):
  return True
