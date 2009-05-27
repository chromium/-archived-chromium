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

"""Set up tools for environments for for software construction toolkit.

This module is a SCons tool which should be include in all environments.  It
will automatically be included by the component_setup tool.
"""


import os
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
      # Use while not if, so we can handle duplicates.
      while vremove in envval:
        envval.remove(vremove)

    self[key] = envval

    # TODO: SCons.Environment.Append() has much more logic to deal with various
    # types of values.  We should handle all those cases in here too.  (If
    # variable is a dict, etc.)

#------------------------------------------------------------------------------


def Overlap(self, values1, values2):
  """Checks for overlap between the values.

  Args:
    self: Environment to use for variable substitution.
    values1: First value(s) to compare.  May be a string or list of strings.
    values2: Second value(s) to compare.  May be a string or list of strings.

  Returns:
    The list of values in common after substitution, or an empty list if
    the values do not overlap.

  Converts the values to a set of plain strings via self.SubstList2() before
  comparison, so SCons $ variables are evaluated.
  """
  set1 = set(self.SubstList2(values1))
  set2 = set(self.SubstList2(values2))
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
  return self.SConscript(sconscript_file, exports={'env': self})

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

  self.SConscript(script_file, exports={'env': self.Clone()})

#------------------------------------------------------------------------------


def SubstList2(self, *args):
  """Replacement subst_list designed for flags/parameters, not command lines.

  Args:
    self: Environment context.
    args: One or more strings or lists of strings.

  Returns:
    A flattened, substituted list of strings.

  SCons's built-in subst_list evaluates (substitutes) variables in its
  arguments, and returns a list of lists (one per positional argument).  Since
  it is designed for use in command line expansion, the list items are
  SCons.Subst.CmdStringHolder instances.  These instances can't be passed into
  env.File() (or subsequent calls to env.subst(), either).  The returned
  nested lists also need to be flattened via env.Flatten() before the caller
  can iterate over the contents.

  SubstList2() does a subst_list, flattens the result, then maps the flattened
  list to strings.

  It is better to do:
    for x in env.SubstList2('$MYPARAMS'):
  than to do:
    for x in env.get('MYPARAMS', []):
  and definitely better than:
    for x in env['MYPARAMS']:
  which will throw an exception if MYPARAMS isn't defined.
  """
  return map(str, self.Flatten(self.subst_list(args)))


#------------------------------------------------------------------------------


def RelativePath(self, source, target, sep=os.sep, source_is_file=False):
  """Calculates the relative path from source to target.

  Args:
    self: Environment context.
    source: Source path or node.
    target: Target path or node.
    sep: Path separator to use in returned relative path.
    source_is_file: If true, calculates the relative path from the directory
        containing the source, rather than the source itself.  Note that if
        source is a node, you can pass in source.dir instead, which is shorter.

  Returns:
    The relative path from source to target.
  """
  # Split source and target into list of directories
  source = self.Entry(str(source))
  if source_is_file:
    source = source.dir
  source = source.abspath.split(os.sep)
  target = self.Entry(str(target)).abspath.split(os.sep)

  # Handle source and target identical
  if source == target:
    if source_is_file:
      return source[-1]         # Bare filename
    else:
      return '.'                # Directory pointing to itself

  # TODO: Handle UNC paths and drive letters (fine if they're the same, but if
  # they're different, there IS no relative path)

  # Remove common elements
  while source and target and source[0] == target[0]:
    source.pop(0)
    target.pop(0)
  # Join the remaining elements
  return sep.join(['..'] * len(source) + target)


#------------------------------------------------------------------------------


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Add methods to environment
  env.AddMethod(ApplySConscript)
  env.AddMethod(BuildSConscript)
  env.AddMethod(FilterOut)
  env.AddMethod(Overlap)
  env.AddMethod(RelativePath)
  env.AddMethod(SubstList2)
