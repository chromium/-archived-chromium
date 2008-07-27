#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

'''Stuff to prevent conflicting shortcuts.
'''

import re

from grit import util


class ShortcutGroup(object):
  '''Manages a list of cliques that belong together in a single shortcut
  group.  Knows how to detect conflicting shortcut keys.
  '''

  # Matches shortcut keys, e.g. &J
  SHORTCUT_RE = re.compile('([^&]|^)(&[A-Za-z])')

  def __init__(self, name):
    self.name = name
    # Map of language codes to shortcut keys used (which is a map of
    # shortcut keys to counts).
    self.keys_by_lang = {}
    # List of cliques in this group
    self.cliques = []
  
  def AddClique(self, c):
    for existing_clique in self.cliques:
      if existing_clique.GetId() == c.GetId():
        # This happens e.g. when we have e.g.
        # <if expr1><structure 1></if> <if expr2><structure 2></if>
        # where only one will really be included in the output.
        return
    
    self.cliques.append(c)
    for (lang, msg) in c.clique.items():
      if lang not in self.keys_by_lang:
        self.keys_by_lang[lang] = {}
      keymap = self.keys_by_lang[lang]
      
      content = msg.GetRealContent()
      keys = [groups[1] for groups in self.SHORTCUT_RE.findall(content)]
      for key in keys:
        key = key.upper()
        if key in keymap:
          keymap[key] += 1
        else:
          keymap[key] = 1
  
  def GenerateWarnings(self, tc_project):
    # For any language that has more than one occurrence of any shortcut,
    # make a list of the conflicting shortcuts.
    problem_langs = {}
    for (lang, keys) in self.keys_by_lang.items():
      for (key, count) in keys.items():
        if count > 1:
          if lang not in problem_langs:
            problem_langs[lang] = []
          problem_langs[lang].append(key)
    
    warnings = []
    if len(problem_langs):
      warnings.append("WARNING - duplicate keys exist in shortcut group %s" %
                      self.name)
      for (lang,keys) in problem_langs.items():
        warnings.append("  %6s duplicates: %s" % (lang, ', '.join(keys)))
    return warnings


def GenerateDuplicateShortcutsWarnings(uberclique, tc_project):
  '''Given an UberClique and a project name, will print out helpful warnings
  if there are conflicting shortcuts within shortcut groups in the provided
  UberClique.
  
  Args:
    uberclique: clique.UberClique()
    tc_project: 'MyProjectNameInTheTranslationConsole'
  
  Returns:
    ['warning line 1', 'warning line 2', ...]
  '''
  warnings = []
  groups = {}
  for c in uberclique.AllCliques():
    for group in c.shortcut_groups:
      if group not in groups:
        groups[group] = ShortcutGroup(group)
      groups[group].AddClique(c)
  for group in groups.values():
    warnings += group.GenerateWarnings(tc_project)  
  return warnings
