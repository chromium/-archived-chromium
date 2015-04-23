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

"""Input gathering tool for SCons."""


import re
import SCons.Script


def GatherInputs(env, target, groups=['.*'], exclude_pattern=None):
  """Find all (non-generated) input files used for a target.

  Args:
    target: a target node to find source files for
      For example: File('bob.exe')
    groups: a list of patterns to use as categories
      For example: ['.*\\.c$', '.*\\.h$']
    exclude_pattern: a pattern to exclude from the search
      For example: '.*third_party.*'
  Returns:
     A list of lists of files for each category.
     Each file will be placed in the first category which matches,
     even if categories overlap.
     For example:
       [['bob.c', 'jim.c'], ['bob.h', 'jim.h']]
  """

  # Compile exclude pattern if any
  if exclude_pattern:
    exclude_pattern = re.compile(exclude_pattern)

  def _FindSources(ptrns, tgt, all):
    """Internal Recursive function to find all pattern matches."""
    # Recursively process lists
    if SCons.Util.is_List(tgt):
      for t in tgt:
        _FindSources(ptrns, t, all)
    else:
      # Get key to use for tracking whether we've seen this node
      target_abspath = None
      if hasattr(tgt, 'abspath'):
        # Use target's absolute path as the key
        target_abspath = tgt.abspath
        target_key = target_abspath
      else:
        # Hope node's representation is unique enough (the default repr
        # contains a pointer to the target as a string).  This works for
        # Alias() nodes.
        target_key = repr(tgt)

      # Skip if we have been here before
      if target_key in all: return
      # Note that we have been here
      all[target_key] = True
      # Skip ones that match an exclude pattern, if we have one.
      if (exclude_pattern and target_abspath
          and exclude_pattern.match(target_abspath)):
        return

      # Handle non-leaf nodes recursively
      lst = tgt.children(scan=1)
      if lst:
        _FindSources(ptrns, lst, all)
        return

      # Get real file (backed by repositories).
      rfile = tgt.rfile()
      rfile_is_file = rfile.isfile()
      # See who it matches
      for pattern, lst in ptrns.items():
        # Add files to the list for the first pattern that matches (implicitly,
        # don't add directories).
        if rfile_is_file and pattern.match(rfile.path):
          lst.append(rfile.abspath)
          break

  # Prepare a group for each pattern.
  patterns = {}
  for g in groups:
    patterns[re.compile(g, re.IGNORECASE)] = []

  # Do the search.
  _FindSources(patterns, target, {})

  return patterns.values()


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Add a method to gather all inputs needed by a target.
  env.AddMethod(GatherInputs, 'GatherInputs')
