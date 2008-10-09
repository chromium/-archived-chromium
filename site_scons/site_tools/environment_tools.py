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

"""Set up tools for environments for for software construction toolkit.

This module is a SCons tool which should be include in all environments.  It
will automatically be included by the component_setup tool.
"""


import SCons


#------------------------------------------------------------------------------


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
      if vremove in envval:
        envval.remove(vremove)

    self[key] = envval

    # TODO(sgk): SCons.Environment.Append() has much more logic to deal
    # with various types of values.  We should handle all those cases in here
    # too.

#------------------------------------------------------------------------------


def Overlap(self, values1, values2):
  """Checks for overlap between the values.

  Args:
    self: Environment to use for variable substitution.
    values1: First value(s) to compare.  May be a string or list of strings.
    values2: Second value(s) to compare.  May be a string or list of strings.

  Returns:
    The list of values in common after substitution, or an empty list if
    the values do no overlap.

  Converts the values to a set of plain strings via self.subst() before
  comparison, so SCons $ variables are evaluated.
  """

  set1 = set()
  for v in self.Flatten(values1):
    set1.add(self.subst(v))

  set2 = set()
  for v in self.Flatten(values2):
    set2.add(self.subst(v))

  return list(set1.intersection(set2))


#------------------------------------------------------------------------------


def ApplySConscript(self, sconscript_file):
  """Applies a SConscript to the current environment.

  Args:
    self: Environment to modify.
    sconscript_file: Name of SConscript file to apply.

  Returns:
    The return value from the call to SConscript().

  ApplySConscript() should be used when an existing SConscript which sets up an
  environment gets too large, or when there is common setup between multiple
  environments which can't be reduced into a parent environment which the
  multiple child environments Clone() from.  The latter case is necessary
  because env.Clone() only enables single inheritance for environments.

  ApplySConscript() is NOT intended to replace the Tool() method.  If you need
  to add methods or builders to one or more environments, do that as a tool
  (and write unit tests for them).

  ApplySConscript() is equivalent to the following SCons call:
      SConscript(sconscript_file, exports={'env':self})

  The called SConscript should import the 'env' variable to get access to the
  calling environment:
      Import('env')

  Changes made to env in the called SConscript will be applied to the
  environment calling ApplySConscript() - that is, env in the called SConscript
  is a reference to the calling environment.

  If you need to export multiple variables to the called SConscript, or return
  variables from it, use the existing SConscript() function.
  """
  return SCons.Script.SConscript(sconscript_file, exports={'env':self})

#------------------------------------------------------------------------------


def BuildSConscript(self, sconscript_file):
  """Builds a SConscript based on the current environment.

  Args:
    self: Environment to clone and pass to the called SConscript.
    sconscript_file: Name of SConscript file to build.  If this is a directory,
        this method will look for sconscript_file+'/build.scons', and if that
        is not found, sconscript_file+'/SConscript'.

  Returns:
    The return value from the call to SConscript().

  BuildSConscript() should be used when an existing SConscript which builds a
  project gets too large, or when a group of SConscripts are logically related
  but should not directly affect each others' environments (for example, a
  library might want to build a number of unit tests which exist in
  subdirectories, but not allow those tests' SConscripts to affect/pollute the
  library's environment.

  BuildSConscript() is NOT intended to replace the Tool() method.  If you need
  to add methods or builders to one or more environments, do that as a tool
  (and write unit tests for them).

  BuildSConscript() is equivalent to the following SCons call:
      SConscript(sconscript_file, exports={'env':self.Clone()})
  or if sconscript_file is a directory:
      SConscript(sconscript_file+'/build.scons', exports={'env':self.Clone()})

  The called SConscript should import the 'env' variable to get access to the
  calling environment:
      Import('env')

  Changes made to env in the called SConscript will NOT be applied to the
  environment calling BuildSConscript() - that is, env in the called SConscript
  is a clone/copy of the calling environment, not a reference to that
  environment.

  If you need to export multiple variables to the called SConscript, or return
  variables from it, use the existing SConscript() function.
  """
  # Need to look for the source node, since by default SCons will look for the
  # entry in the variant_dir, which won't exist (and thus won't be a directory
  # or a file).  This isn't a problem in BuildComponents(), since the variant
  # dir is only set inside its call to SConscript().
  if self.Entry(sconscript_file).srcnode().isdir():
    # Building a subdirectory, so look for build.scons or SConscript
    script_file = sconscript_file + '/build.scons'
    if not self.File(script_file).srcnode().exists():
      script_file = sconscript_file + '/SConscript'
  else:
    script_file = sconscript_file

  self.SConscript(script_file, exports={'env':self.Clone()})

#------------------------------------------------------------------------------


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Add methods to environment
  env.AddMethod(ApplySConscript)
  env.AddMethod(BuildSConscript)
  env.AddMethod(FilterOut)
  env.AddMethod(Overlap)
