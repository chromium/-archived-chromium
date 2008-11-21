# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tool module for managing conditional loading of components in a
Chromium build (i.e., the LOAD= command-line argument).
"""

from SCons.Script import *

def generate(env):

  class LoadComponent:
    """
    Class for deciding if a given component name is to be included
    based on a list of included names, optionally prefixed with '-'
    to exclude the name.
    """
    def __init__(self, load=[], names=[]):
      """
      Initialize a class with a list of names for possible loading.

      Arguments:
        load:  list of elements in the LOAD= specification
        names:  name(s) that represent this global collection
      """
      self.included = set([c for c in load if not c.startswith('-')])
      self.excluded = set([c[1:] for c in load if c.startswith('-')])

      # Remove the global collection name(s) from the included list.
      # This supports (e.g.) specifying 'webkit' so the top-level of the
      # hierarchy will read up the subsystem, and then 'test_shell'
      # as one specific sub-component within 'webkit'.
      self.included = self.included - set(names)

      if not self.included:
        self.included = ['all']

    def __call__(self, component):
      """
      Returns True if the specified component should be loaded,
      based on the initialized included and excluded lists.
      """
      return (component in self.included or
              ('all' in self.included and not component in self.excluded))

  def ChromiumLoadComponentSConscripts(env, *args, **kw):
    """
    Returns a list of SConscript files to load, based on LOAD=.

    SConscript files specified without keyword arguments are returned
    unconditionally.  SConscript files specified with a keyword arguments
    (e.g. chrome = 'chrome.scons') are loaded only if the LOAD= line
    indicates their keyword argument should be included in the load.
    """
    try:
        load = ARGUMENTS.get('LOAD').split(',')
    except AttributeError:
        load = []

    try:
        names = kw['LOAD_NAMES']
    except KeyError:
        names = []
    else:
        del kw['LOAD_NAMES']

    load_component = LoadComponent(load, names)

    result = list(args)
    for module, sconscript in kw.items():
      if load_component(module):
        result.append(sconscript)

    return Flatten(result)

  env.AddMethod(ChromiumLoadComponentSConscripts)

def exists(env):
  return True
