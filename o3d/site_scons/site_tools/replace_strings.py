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

"""Search and replace builder for SCons."""


import re
import SCons.Script


def ReplaceStrings(target, source, env):
  """Replace Strings builder, does regex substitution on files.

  Args:
    target: A single target file node.
    source: A single input file node.
    env: Environment in which to build.

  From env:
    REPLACE_STRINGS: A list of pairs of regex search and replacement strings.
        The body of the source file has substitution performed on each
        pair (search_regex, replacement) in order.  SCons variables in the
        replacement strings will be evaluated.

  Returns:
    The target node, a file with contents from source, with the substitutions
    from REPLACE_STRINGS performed on it.

  For example:
    env.ReplaceStrings('out', 'in',
                       REPLACE_STRINGS = [('a*', 'b'), ('b', 'CCC')])
    With 'in' having contents: Haaapy.
    Outputs: HCCCpy.
  """
  # Load text.
  fh = open(source[0].abspath, 'rb')
  text = fh.read()
  fh.close()
  # Do replacements.
  for r in env['REPLACE_STRINGS']:
    text = re.sub(r[0], env.subst(r[1]), text)
  # Write it out.
  fh = open(target[0].abspath, 'wb')
  fh.write(text)
  fh.close()


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Add the builder
  act = SCons.Script.Action(ReplaceStrings, varlist=['REPLACE_STRINGS'])
  bld = SCons.Script.Builder(action=act, single_source=True)
  env.Append(BUILDERS={'ReplaceStrings': bld})
