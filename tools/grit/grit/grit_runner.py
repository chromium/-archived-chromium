#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Command processor for GRIT.  This is the script you invoke to run the various
GRIT tools.
'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import getopt

from grit import util

import grit.exception

import grit.tool.build
import grit.tool.count
import grit.tool.diff_structures
import grit.tool.menu_from_parts
import grit.tool.newgrd
import grit.tool.resize
import grit.tool.rc2grd
import grit.tool.test
import grit.tool.transl2tc
import grit.tool.unit


# Copyright notice
_COPYRIGHT = '''\
GRIT - the Google Resource and Internationalization Tool
Copyright (c) Google Inc. %d
''' % util.GetCurrentYear()

# Keys for the following map
_CLASS = 1
_REQUIRES_INPUT = 2
_HIDDEN = 3  # optional key - presence indicates tool is hidden


# Maps tool names to the tool's module.  Done as a list of (key, value) tuples
# instead of a map to preserve ordering.
_TOOLS = [
  ['build', { _CLASS : grit.tool.build.RcBuilder, _REQUIRES_INPUT : True }],
  ['newgrd', { _CLASS  : grit.tool.newgrd.NewGrd, _REQUIRES_INPUT : False }],
  ['rc2grd', { _CLASS : grit.tool.rc2grd.Rc2Grd, _REQUIRES_INPUT : False }],
  ['transl2tc', { _CLASS : grit.tool.transl2tc.TranslationToTc,
                 _REQUIRES_INPUT : False }],
  ['sdiff', { _CLASS : grit.tool.diff_structures.DiffStructures,
                       _REQUIRES_INPUT : False }],
  ['resize', { _CLASS : grit.tool.resize.ResizeDialog, _REQUIRES_INPUT : True }],
  ['unit', { _CLASS : grit.tool.unit.UnitTestTool, _REQUIRES_INPUT : False }],
  ['count', { _CLASS : grit.tool.count.CountMessage, _REQUIRES_INPUT : True }],
  ['test', { _CLASS: grit.tool.test.TestTool, _REQUIRES_INPUT : True, _HIDDEN : True }],
  ['menufromparts', { _CLASS: grit.tool.menu_from_parts.MenuTranslationsFromParts,
                     _REQUIRES_INPUT : True, _HIDDEN : True }],
]


def PrintUsage():
  print _COPYRIGHT

  tool_list = ''
  for (tool, info) in _TOOLS:
    if not _HIDDEN in info.keys():
      tool_list += '    %-12s %s\n' % (tool, info[_CLASS]().ShortDescription())

  # TODO(joi) Put these back into the usage when appropriate:
  #
  #  -d    Work disconnected.  This causes GRIT not to attempt connections with
  #        e.g. Perforce.
  #
  #  -c    Use the specified Perforce CLIENT when talking to Perforce.
  print '''Usage: grit [GLOBALOPTIONS] TOOL [args to tool]

Global options:

  -i INPUT  Specifies the INPUT file to use (a .grd file).  If this is not
            specified, GRIT will look for the environment variable GRIT_INPUT.
            If it is not present either, GRIT will try to find an input file
            named 'resource.grd' in the current working directory.

  -v        Print more verbose runtime information.

  -x        Print extremely verbose runtime information.  Implies -v

  -p FNAME  Specifies that GRIT should profile its execution and output the
            results to the file FNAME.

Tools:

  TOOL can be one of the following:
%s
  For more information on how to use a particular tool, and the specific
  arguments you can send to that tool, execute 'grit help TOOL'
''' % (tool_list)


class Options(object):
  '''Option storage and parsing.'''

  def __init__(self):
    self.disconnected = False
    self.client = ''
    self.input = None
    self.verbose = False
    self.extra_verbose = False
    self.output_stream = sys.stdout
    self.profile_dest = None

  def ReadOptions(self, args):
    '''Reads options from the start of args and returns the remainder.'''
    (opts, args) = getopt.getopt(args, 'g:dvxc:i:p:')
    for (key, val) in opts:
      if key == '-d': self.disconnected = True
      elif key == '-c': self.client = val
      elif key == '-i': self.input = val
      elif key == '-v':
        self.verbose = True
        util.verbose = True
      elif key == '-x':
        self.verbose = True
        util.verbose = True
        self.extra_verbose = True
        util.extra_verbose = True
      elif key == '-p': self.profile_dest = val

    if not self.input:
      if 'GRIT_INPUT' in os.environ:
        self.input = os.environ['GRIT_INPUT']
      else:
        self.input = 'resource.grd'

    return args

  def __repr__(self):
    return '(disconnected: %d, verbose: %d, client: %s, input: %s)' % (
      self.disconnected, self.verbose, self.client, self.input)


def _GetToolInfo(tool):
  '''Returns the info map for the tool named 'tool' or None if there is no
  such tool.'''
  matches = filter(lambda t: t[0] == tool, _TOOLS)
  if not len(matches):
    return None
  else:
    return matches[0][1]


def Main(args):
  '''Parses arguments and does the appropriate thing.'''
  util.ChangeStdoutEncoding()

  if not len(args) or len(args) == 1 and args[0] == 'help':
    PrintUsage()
    return 0
  elif len(args) == 2 and args[0] == 'help':
    tool = args[1].lower()
    if not _GetToolInfo(tool):
      print "No such tool.  Try running 'grit help' for a list of tools."
      return 2

    print ("Help for 'grit %s' (for general help, run 'grit help'):\n"
           % (tool))
    print _GetToolInfo(tool)[_CLASS].__doc__
    return 0
  else:
    options = Options()
    args = options.ReadOptions(args)  # args may be shorter after this
    tool = args[0]
    if not _GetToolInfo(tool):
      print "No such tool.  Try running 'grit help' for a list of tools."
      return 2

    try:
      if _GetToolInfo(tool)[_REQUIRES_INPUT]:
        os.stat(options.input)
    except OSError:
      print ('Input file %s not found.\n'
             'To specify a different input file:\n'
             '  1. Use the GRIT_INPUT environment variable.\n'
             '  2. Use the -i command-line option.  This overrides '
             'GRIT_INPUT.\n'
             '  3. Specify neither GRIT_INPUT or -i and GRIT will try to load '
             "'resource.grd'\n"
             '     from the current directory.' % options.input)
      return 2

    toolobject = _GetToolInfo(tool)[_CLASS]()
    if options.profile_dest:
      import hotshot
      prof = hotshot.Profile(options.profile_dest)
      prof.runcall(toolobject.Run, options, args[1:])
    else:
      toolobject.Run(options, args[1:])


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

