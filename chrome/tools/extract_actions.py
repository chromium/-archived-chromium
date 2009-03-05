#!/usr/bin/python
# Copyright 2007 Google Inc.  All rights reserved.

"""Extract UserMetrics "actions" strings from the Chrome source.

This program generates the list of known actions we expect to see in the
user behavior logs.  It walks the Chrome source, looking for calls to
UserMetrics functions, extracting actions and warning on improper calls,
as well as generating the lists of possible actions in situations where
there are many possible actions.

See also:
  chrome/browser/user_metrics.h
  http://wiki.corp.google.com/twiki/bin/view/Main/ChromeUserExperienceMetrics

Run it from the chrome/browser directory like:
  extract_actions.py > actions_list
"""

__author__ = 'evanm (Evan Martin)'

import os
import re
import sys

from google import path_utils

# Files that are known to use UserMetrics::RecordComputedAction(), which means
# they require special handling code in this script.
# To add a new file, add it to this list and add the appropriate logic to
# generate the known actions to AddComputedActions() below.
KNOWN_COMPUTED_USERS = [
  'back_forward_menu_model.cc',
  'options_page_view.cc',
  'render_view_host.cc',  # called using webkit identifiers
  'user_metrics.cc',  # method definition
  'new_tab_ui.cc',  # most visited clicks 1-9
]

def AddComputedActions(actions):
  """Add computed actions to the actions list.

  Arguments:
    actions: set of actions to add to.
  """

  # Actions for back_forward_menu_model.cc.
  for dir in ['BackMenu_', 'ForwardMenu_']:
    actions.add(dir + 'ShowFullHistory')
    actions.add(dir + 'Popup')
    for i in range(1, 20):
      actions.add(dir + 'HistoryClick' + str(i))
      actions.add(dir + 'ChapterClick' + str(i))

  # Actions for new_tab_ui.cc.
  for i in range(1, 10):
    actions.add('MostVisited%d' % i)

def AddWebKitEditorActions(actions):
  """Add editor actions from editor_client_impl.cc.

  Arguments:
    actions: set of actions to add to.
  """
  action_re = re.compile(r'''\{ [\w']+, +\w+, +"(.*)" +\},''')

  editor_file = os.path.join(path_utils.ScriptDir(), '..', '..', 'webkit',
                             'glue', 'editor_client_impl.cc')
  for line in open(editor_file):
    match = action_re.search(line)
    if match:  # Plain call to RecordAction
      actions.add(match.group(1))

def GrepForActions(path, actions):
  """Grep a source file for calls to UserMetrics functions.

  Arguments:
    path: path to the file
    actions: set of actions to add to
  """

  action_re = re.compile(r'[> ]UserMetrics:?:?RecordAction\(L"(.*)"')
  other_action_re = re.compile(r'[> ]UserMetrics:?:?RecordAction\(')
  computed_action_re = re.compile(r'UserMetrics::RecordComputedAction')
  for line in open(path):
    match = action_re.search(line)
    if match:  # Plain call to RecordAction
      actions.add(match.group(1))
    elif other_action_re.search(line):
      # Warn if this file shouldn't be mentioning RecordAction.
      if os.path.basename(path) != 'user_metrics.cc':
        print >>sys.stderr, 'WARNING: %s has funny RecordAction' % path
    elif computed_action_re.search(line):
      # Warn if this file shouldn't be calling RecordComputedAction.
      if os.path.basename(path) not in KNOWN_COMPUTED_USERS:
        print >>sys.stderr, 'WARNING: %s has RecordComputedAction' % path

def WalkDirectory(root_path, actions):
  for path, dirs, files in os.walk(root_path):
    if '.svn' in dirs:
      dirs.remove('.svn')
    for file in files:
      ext = os.path.splitext(file)[1]
      if ext == '.cc':
        GrepForActions(os.path.join(path, file), actions)

def main(argv):
  actions = set()
  AddComputedActions(actions)
  AddWebKitEditorActions(actions)

  # Walk the source tree to process all .cc files.
  chrome_root = os.path.join(path_utils.ScriptDir(), '..')
  WalkDirectory(chrome_root, actions)
  webkit_root = os.path.join(path_utils.ScriptDir(), '..', '..', 'webkit')
  WalkDirectory(os.path.join(webkit_root, 'glue'), actions)
  WalkDirectory(os.path.join(webkit_root, 'port'), actions)

  # Print out the actions as a sorted list.
  for action in sorted(actions):
    print action

if '__main__' == __name__:
  main(sys.argv)
