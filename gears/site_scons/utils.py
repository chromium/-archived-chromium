# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def PatternRule(target, source, env):
  """Apply env substitution to a target with $SOURCE included.  Returns a list
  containing the new target and source to pass to a builder."""
  target_sub = env.subst(target, source=env.File(source))
  return [target_sub, source]

def GetInputs(var, env):
  """Expands an env substitution variable and returns it as a list of
  strings."""
  return [str(v) for v in env.subst_list(var)[0]]
